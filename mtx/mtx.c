/*  smc - simple matrix client
 *
 *  Copyright (C) 2020 Jona Ackerschott
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include <curl/curl.h>
#include <json-c/json.h>

#include "lib/hjson.h"
#include "lib/list.h"
#include "mtx/devices.h"
#include "mtx/encryption.h"
#include "mtx/mtx.h"
#include "mtx/signing.h"
#include "mtx/state/parse.h"
#include "mtx/state/apply.h"
#include "mtx/state/room.h"

static CURL *handle;

#define API_ERRORMSG_BUFSIZE 256U
static int lastcode;
static mtx_error_t lasterror;
static char last_api_error_msg[API_ERRORMSG_BUFSIZE];

struct mtx_session_t {
	char *hostname;
	char *accesstoken;
	char *userid;

	device_t *device;
	mtx_listentry_t devices;
	OlmAccount *olmaccount;

	char *nextbatch;
	mtx_listentry_t joinedrooms;
	mtx_listentry_t invitedrooms;
	mtx_listentry_t leftrooms;
};

typedef enum {
	MTX_ID_USER,
	MTX_ID_THIRD_PARTY,
	MTX_ID_PHONE,
} mtx_id_type_t;
union mtx_id_t {
	mtx_id_type_t type;
	struct {
		mtx_id_type_t type;
		char *name;
	} user;
	struct {
		mtx_id_type_t type;
		char *medium;
		char *address;
	} thirdparty;
	struct {
		mtx_id_type_t type;
		char *country;
		char *number;
	} phone;
};

struct mtx_sync_response_t {
	json_object *resp;
};

#define RESP_READSIZE 4096U
typedef struct {
	size_t size;
	size_t len;
	char *data;
} respbuf_t;
static size_t hrecv(void *buf, size_t sz, size_t n, void *data)
{
	respbuf_t *resp = (respbuf_t *)data;

	size_t remsize = resp->size - resp->len;
	if (n > remsize) {
		size_t newsize = (n / RESP_READSIZE + 1) * RESP_READSIZE + resp->size;
		char *p = realloc(resp->data, newsize);
		if (!p)
			return 0;
		resp->data = p;
		resp->size = newsize;
	}
	char *c = resp->data + resp->len;

	memcpy(c, buf, n);
	resp->len += n;
	return n;
}

#define URL_BUFSIZE 1024U
static void reset_handle(CURL *handle)
{
	curl_easy_reset(handle);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, hrecv);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0); /* for testing */
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0); /* for testing */
	//curl_easy_setopt(handle, CURLOPT_VERBOSE, 1); /* for testing */
}
static void get_response_error(const json_object *resp,
		int code, mtx_error_t *error, char *errormsg)
{
	int apierr;
	json_get_object_as_enum_(resp, "errcode", &apierr, MTX_ERR_NUM, mtx_api_error_strs);
	*error = ((mtx_error_t)apierr) + MTX_ERR_M_UNKNOWN;

	json_object *obj;
	json_object_object_get_ex(resp, "error", &obj);
	strcpy(errormsg, "api error: ");
	strcat(errormsg, json_object_get_string(obj));
	errormsg[0] = tolower(errormsg[0]);
}
static int api_call(const char *hostname, const char *request, const char *target,
		const char *urlparams, json_object *data, int *code, json_object **response)
{
	reset_handle(handle);

	if (request)
		curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, request);

	char url[URL_BUFSIZE];
	strcpy(url, "https://");
	strcat(url, hostname);
	strcat(url, target);
	if (urlparams) {
		strcat(url, "?");
		strcat(url, urlparams);
	}
	curl_easy_setopt(handle, CURLOPT_URL, url);

	struct curl_slist *header = NULL;
        header = curl_slist_append(header, "Content-type: application/json");
	header = curl_slist_append(header, "Accept: application/json");
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header);

	if (data) {
		const char *reqdata = json_object_to_json_string(data);
		curl_easy_setopt(handle, CURLOPT_POSTFIELDS, reqdata);
	}

	char *respdata = malloc(RESP_READSIZE);
	if (!respdata) {
		curl_slist_free_all(header);
		goto err_local;
	}
	respbuf_t respbuf;
	respbuf.size = RESP_READSIZE;
	respbuf.len = 0;
	respbuf.data = respdata;
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &respbuf);

	CURLcode err = curl_easy_perform(handle);
	if (err) {
		curl_slist_free_all(header);
		free(respbuf.data);
		goto err_local;
	}
	curl_slist_free_all(header);
	respbuf.data[respbuf.len] = 0;

	int c;
	curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &c);

	json_object *resp = json_tokener_parse(respbuf.data);
	if (!resp) {
		free(respbuf.data);
		goto err_local;
	}
	free(respbuf.data);
	*response = resp;
	*code = c;

	lastcode = c;
	if (c != 200) {
		get_response_error(resp, c, &lasterror, last_api_error_msg);
		return 1;
	} else {
		lasterror = MTX_ERR_SUCCESS;
	}

	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}

#define TRANSACTION_ID_SIZE 44
static int generate_transaction_id(char *id)
{
	const size_t sz = (TRANSACTION_ID_SIZE / 4) * 3;
	uint8_t rdbuf[sz];
	if (getrandom_(rdbuf, sz))
		return 1;

	uint8_t dest[TRANSACTION_ID_SIZE];
	base64url(rdbuf, sz, dest);
	for (int i = 0; i < ARRNUM(dest); ++i) {
		id[i] = dest[i];
	}
	return 0;
}

static void free_olm_account(OlmAccount *acc)
{
	if (!acc)
		return;

	olm_clear_account(acc);
	free(acc);
}
static OlmAccount *create_olm_account(void)
{
	OlmAccount *acc = malloc(olm_account_size());
	if (!acc)
		return NULL;
	acc = olm_account(acc);

	size_t rdlen = olm_create_account_random_length(acc);
	void *random = malloc(rdlen);
	if (!random) {
		free(acc);
		return NULL;
	}

	if (getrandom_(random, rdlen)) {
		free(random);
		free(acc);
		return NULL;
	}

	if (olm_create_account(acc, random, rdlen) == olm_error()) {
		free(random);
		free(acc);
		return NULL;
	}
	free(random);

	return acc;
}

int mtx_init(void)
{
	if (curl_global_init(CURL_GLOBAL_SSL))
		return 1;

	handle = curl_easy_init();
	if (!handle) {
		curl_global_cleanup();
		return 1;
	}
	reset_handle(handle);

	return 0;
}
void mtx_cleanup(void)
{
	curl_easy_cleanup(handle);
	curl_global_cleanup();
}
mtx_error_t mtx_last_error(void)
{
	return lasterror;
}
char *mtx_last_error_msg(void)
{
	char *msg;
	if (lasterror >= MTX_ERR_M_UNKNOWN) {
		msg = strdup(last_api_error_msg);
	} else if (lasterror == MTX_ERR_LOCAL) {
		msg = strdup("local error");
	} else if (lasterror == MTX_ERR_SUCCESS) {
		msg = strdup("success");
	} else {
		assert(0);
	}
	if (!msg)
		return NULL;
	
	return msg;
}

void mtx_free_session(mtx_session_t *session)
{
	if (!session)
		return;

	free(session->hostname);
	free(session->userid);
	free(session->accesstoken);

	mtx_list_free(&session->devices, device_list_t, entry, free_device_list);
	free_device(session->device);
	free_olm_account(session->olmaccount);

	free(session->nextbatch);
	mtx_list_free(&session->joinedrooms, mtx_room_t, entry, free_room);
	mtx_list_free(&session->invitedrooms, mtx_room_t, entry, free_room);
	mtx_list_free(&session->leftrooms, mtx_room_t, entry, free_room);

	free(session);
}
mtx_session_t *mtx_new_session()
{
	mtx_session_t *session = malloc(sizeof(*session));
	if (!session)
		goto err_local;
	memset(session, 0, sizeof(*session));

	mtx_list_init(&session->devices);
	mtx_list_init(&session->joinedrooms);
	mtx_list_init(&session->invitedrooms);
	mtx_list_init(&session->leftrooms);

	return session;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return NULL;
}

void mtx_free_id(mtx_id_t *id)
{
	if (id->type == MTX_ID_USER) {
		free(id->user.name);
	} else if (id->type == MTX_ID_THIRD_PARTY) {
		free(id->thirdparty.medium);
		free(id->thirdparty.address);
	} else if (id->type == MTX_ID_PHONE) {
		free(id->phone.country);
		free(id->phone.number);
	}
	free(id);
}
mtx_id_t *mtx_create_id_user(const char *username)
{
	mtx_id_t *id = malloc(sizeof(*id));
	if (!id)
		return NULL;

	id->type = MTX_ID_USER;

	char *name = strdup(username);
	if (!name) {
		free(id);
		return NULL;
	}
	id->user.name = name;

	return id;
}
mtx_id_t *mtx_create_id_third_party(const char *medium, const char *address)
{
	mtx_id_t *id = malloc(sizeof(*id));
	if (!id)
		return NULL;

	id->type = MTX_ID_THIRD_PARTY;

	char *med = strdup(medium);
	if (!med) {
		free(id);
		return NULL;
	}
	id->thirdparty.medium = med;

	char *addr = strdup(address);
	if (!addr) {
		free(med);
		free(id);
		return NULL;
	}
	id->thirdparty.address = addr;

	return id;
}
mtx_id_t *mtx_create_id_phone(const char *country, const char *number)
{
	mtx_id_t *id = malloc(sizeof(*id));
	if (!id)
		return NULL;
	
	id->type = MTX_ID_PHONE;

	char *ctry = strdup(country);
	if (!ctry) {
		free(id);
		return NULL;
	}
	id->phone.country = ctry;

	char *num = strdup(number);
	if (!num) {
		free(ctry);
		free(id);
		return NULL;
	}
	id->phone.number = num;

	return id;
}

static int login_data_add_id_user(json_object *data, mtx_id_t *id)
{
	json_object *ident;
	if (json_add_object_(data, "identifier", &ident))
		return 1;

	if (json_add_string_(ident, "type", "m.id.user"))
		return 1;

	if (json_add_string_(ident, "user", id->user.name))
		return 1;

	return 0;
}
static int login_data_add_id_third_party(json_object *data, mtx_id_t *id)
{
	json_object *ident;
	if (json_add_object_(data, "identifier", &ident))
		return 1;

	if (json_add_string_(ident, "type", "m.id.thirdparty"))
		return 1;

	if (json_add_string_(ident, "medium", id->thirdparty.medium))
		return 1;

	if (json_add_string_(ident, "address", id->thirdparty.address))
		return 1;

	return 0;
}
static int login_data_add_id_phone(json_object *data, mtx_id_t *id)
{
	json_object *ident;
	if (json_add_object_(data, "identifier", &ident))
		return 1;

	if (json_add_string_(ident, "type", "m.id.phone"))
		return 1;

	if (json_add_string_(ident, "country", id->phone.country))
		return 1;

	if (json_add_string_(ident, "phone", id->phone.number))
		return 1;

	return 0;
}
static char *get_homeserver(const char *userid)
{
	const char *c = strchr(userid, ':');
	assert(c && strlen(c) > 1);

	return strdup(c + 1);
}
static int login(mtx_session_t *session, const char *hostname, const char *typestr, mtx_id_t *id,
		const char *secret, const char *_devid, const char *devname)
{
	json_object *data = json_object_new_object();
	if (!data)
		goto err_local;

	if (json_add_string_(data, "type", typestr)) {
		json_object_put(data);
		goto err_local;
	}
	if (_devid && json_add_string_(data, "device_id", _devid)) {
		json_object_put(data);
		goto err_local;
	}
	if (devname && json_add_string_(data, "initial_device_display_name", devname)) {
		json_object_put(data);
		goto err_local;
	}

	assert(strcmp(typestr, "m.login.password") == 0 || strcmp(typestr, "m.login.token") == 0);
	if (strcmp(typestr, "m.login.password") == 0
			&& json_add_string_(data, "password", secret)) {
		json_object_put(data);
		goto err_local;
	}
	if (strcmp(typestr, "m.login.token") == 0
			&& json_add_string_(data, "token", secret)) {
		json_object_put(data);
		goto err_local;
	}

	int err;
	switch (id->type) {
	case MTX_ID_USER:
		err = login_data_add_id_user(data, id);
		break;
	case MTX_ID_THIRD_PARTY:
		err = login_data_add_id_third_party(data, id);
		break;
	case MTX_ID_PHONE:
		err = login_data_add_id_phone(data, id);
		break;
	default:
		assert(0);
	}
	if (err) {
		json_object_put(data);
		goto err_local;
	}

	int code;
	json_object *resp;
	if (api_call(hostname, "POST", "/_matrix/client/r0/login", NULL, data, &code, &resp)) {
		json_object_put(data);
		return 1;
	}
	json_object_put(data);

	char *devid = NULL;
	if (json_get_object_as_string_(resp, "access_token", &session->accesstoken)
			|| json_get_object_as_string_(resp, "user_id", &session->userid)
			|| json_get_object_as_string_(resp, "device_id", &devid)) {
		json_object_put(resp);
		goto err_local;
	}
	json_object_put(resp);

	char *hs = get_homeserver(session->userid);
	if (!hs)
		goto err_local;
	session->hostname = hs;

	OlmAccount *acc = create_olm_account();
	if (!acc)
		goto err_local;
	session->olmaccount = acc;

	device_t *device = create_device(session->olmaccount, devid);
	if (!device)
		goto err_local;
	session->device = device;

	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
int mtx_login_password(mtx_session_t *session, const char *hostname, mtx_id_t *id, const char *pass,
		const char *devid, const char *devname)
{
	return login(session, hostname, "m.login.password", id, pass, devid, devname);
}
int mtx_login_token(mtx_session_t *session, const char *hostname, mtx_id_t *id, const char *token,
		const char *devid, const char *devname)
{
	return login(session, hostname, "m.login.token", id, token, devid, devname);
}
const char *mtx_accesstoken(mtx_session_t *session)
{
	return session->accesstoken;
}
const char *mtx_device_id(mtx_session_t *session)
{
	return session->device->id;
}

static int query_user_id(const char *hostname, const char *accesstoken, char **userid)
{
	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, accesstoken);

	int code;
	json_object *resp;
	if (api_call(hostname, "GET", "/_matrix/client/r0/account/whoami",
			urlparams, NULL, &code, &resp))
		return 1;

	*userid = NULL;
	if (json_get_object_as_string_(resp, "user_id", userid)) {
		json_object_put(resp);
		goto err_local;
	}
	json_object_put(resp);

	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
int mtx_recall_past_session(mtx_session_t *session, const char *hostname,
		const char *accesstoken, const char *devid)
{
	char *userid;
	if (query_user_id(hostname, accesstoken, &userid))
		return 1;

	char *hs = get_homeserver(userid);
	if (!hs)
		goto err_local;

	if (strrpl(&session->hostname, hs))
		goto err_local;
	free(hs);

	if (strrpl(&session->accesstoken, accesstoken))
		goto err_local;

	if (strrpl(&session->userid, userid))
		goto err_local;
	free(userid);

	OlmAccount *acc = create_olm_account();
	if (!acc)
		goto err_local;
	session->olmaccount = acc;

	device_t *device = create_device(session->olmaccount, devid);
	if (!device)
		goto err_local;
	session->device = device;

	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}

static int upload_keys(const mtx_session_t *session)
{
	json_object *data = json_object_new_object();
	if (!data)
		goto err_local;

	json_object *devkeys = json_object_new_object();
	if (!devkeys)
		goto err_free_data;
	if (json_object_object_add(data, "device_keys", devkeys)) {
		json_object_put(devkeys);
		goto err_free_data;
	}
	if (json_add_string_(devkeys, "user_id", session->userid)
			|| json_add_string_(devkeys, "device_id", session->device->id)
			|| json_add_string_array_(devkeys, "algorithms",
					ARRNUM(crypto_algorithms_msg), crypto_algorithms_msg)) {
		goto err_free_data;
	}

	json_object *keys = device_keys_to_export_format(
			session->device->identkeys, session->device->id);
	if (!keys)
		goto err_free_data;

	if (json_object_object_add(devkeys, "keys", keys)) {
		json_object_put(keys);
		goto err_free_data;
	}


	if (sign_json(session->olmaccount, devkeys, session->userid, session->device->id))
		goto err_free_data;

	/* one time keys */
	json_object *otkeys = json_object_new_object();
	if (!otkeys)
		goto err_free_data;

	if (json_object_object_add(data, "one_time_keys", otkeys)) {
		json_object_put(otkeys);
		goto err_free_data;
	}

	// TODO: implement rest of one time keys


	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, session->accesstoken);

	int code;
	json_object *resp;
	if (api_call(session->hostname, "POST", "/_matrix/client/r0/keys/upload",
				urlparams, data, &code, &resp)) {
		json_object_put(data);
		return 1;
	}
	json_object_put(data);

	printf("%s\n", json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PRETTY));

	json_object_put(resp);
	return 0;

err_free_data:
	json_object_put(data);
err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
static int query_keys(mtx_session_t *session, const char *sincetoken, int timeout)
{
	json_object *data = json_object_new_object();
	if (!data)
		goto err_local;

	if (json_add_int_(data, "timeout", timeout)) {
		json_object_put(data);
		goto err_local;
	}

	if (sincetoken && json_add_string_(data, "token", sincetoken)) {
		json_object_put(data);
		goto err_local;
	}

	json_object *devkeys;
	if (json_add_object_(data, "device_keys", &devkeys)) {
		json_object_put(data);
		goto err_local;
	}

	int updates = 0;
	for (mtx_listentry_t *e = session->devices.next; e != &session->devices; e = e->next) {
		device_list_t *devlist = mtx_list_entry_content(e, device_list_t, entry);

		if (!devlist->dirty)
			continue;

		json_object *_devlist;
		if (json_add_array_(data, devlist->owner, &_devlist)) {
			json_object_put(data);
			goto err_local;
		}

		mtx_listentry_t *devs = &devlist->devices;
		for (mtx_listentry_t *f = devs->next; f != devs; f = f->next) {
			device_t *dev = mtx_list_entry_content(devs, device_t, entry);

			if (json_array_add_string_(_devlist, dev->id)) {
				json_object_put(data);
				goto err_local;
			}
		}

		updates = 1;
	}
	if (!updates) {
		json_object_put(data);
		return 0;
	}

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, session->accesstoken);

	int code;
	json_object *resp;
	if (api_call(session->hostname, "POST", "/_matrix/client/r0/keys/query",
				urlparams, data, &code, &resp)) {
		json_object_put(data);
		return 1;
	}
	json_object_put(data);

	// TODO: Handle failures

	json_object *rdevkeys;
	json_object_object_get_ex(resp, "device_keys", &rdevkeys);
	if (!devkeys) {
		json_object_put(resp);
		goto err_local;
	}

	json_object_object_foreach(rdevkeys, userid, devinfos) {
		json_object_object_foreach(devinfos, devid, devinfo) {
			if (update_device(&session->devices, userid, devid, devinfo)) {
				json_object_put(resp);
				goto err_local;
			}
		}
	}

	json_object_put(resp);

	for (mtx_listentry_t *e = session->devices.next; e != &session->devices; e = e->next) {
		device_list_t *devlist = mtx_list_entry_content(e, device_list_t, entry);
		devlist->dirty = 0;
	}

	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
static int query_key_changes(mtx_session_t *session, const char *from, const char *to)
{
	json_object *data = json_object_new_object();
	if (!data)
		goto err_local;

	if (json_add_string_(data, "from", from)
			|| json_add_string_(data, "to", to))
		goto err_local;

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, session->accesstoken);

	int code;
	json_object *resp;
	if (api_call(session->hostname, "POST", "/_matrix/client/r0/keys/changes",
				urlparams, data, &code, &resp)) {
		json_object_put(data);
		return 1;
	}
	json_object_put(data);

	if (update_device_lists(&session->devices, resp))
		goto err_local;

	json_object_put(resp);
	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 0;
}
int mtx_exchange_keys(mtx_session_t *session, const mtx_listentry_t *devtrackinfos,
		const char *sincetoken, int timeout)
{
	assert(session->accesstoken);

	if (upload_keys(session))
		return 1;

	if (init_device_lists(&session->devices, devtrackinfos))
		goto err_local;

	if (query_keys(session, sincetoken, timeout))
		return 1;

	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}

int mtx_sync(const mtx_session_t *session, int timeout, mtx_sync_response_t **_response)
{
	assert(session->accesstoken);

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, session->accesstoken);

	if (session->nextbatch) {
		strcat(urlparams, "&since=");
		strcat(urlparams, session->nextbatch);
	}

	strcat(urlparams, "&timeout=");

	char t[16];
	sprintf(t, "%i", timeout);
	strcat(urlparams, t);

	int code;
	json_object *resp;
	if (api_call(session->hostname, "GET", "/_matrix/client/r0/sync",
				urlparams, NULL, &code, &resp))
		return 1;

	mtx_sync_response_t *response = malloc(sizeof(*response));
	if (!response) {
		json_object_put(resp);
		goto err_local;
	}
	response->resp = resp;

	*_response = response;
	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
int mtx_apply_sync(mtx_session_t *session, mtx_sync_response_t *response)
{
	json_object *resp = response->resp;

	char *nextbatch = NULL;
	if (json_get_object_as_string_(resp, "next_batch", &nextbatch))
		goto err_local;
	if (strrpl(&session->nextbatch, nextbatch))
		goto err_local;
	free(nextbatch);

	json_object *rooms;
	if (json_object_object_get_ex(resp, "rooms", &rooms)) {
		if (update_room_histories(rooms, &session->joinedrooms,
					&session->invitedrooms, &session->leftrooms)) {
			goto err_local;
		}
	}

	json_object *devlists;
	if (json_object_object_get_ex(resp, "device_lists", &devlists)) {
		if (update_device_lists(&session->devices, devlists))
			goto err_local;
	}

	json_object_put(resp);
	free(response);
	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
const char *mtx_get_since_token(mtx_session_t *session)
{
	return session->nextbatch;
}
int mtx_get_device_tracking_infos(mtx_session_t *session, mtx_listentry_t *devtrackinfos)
{
	if (get_device_tracking_infos(&session->devices, devtrackinfos)) {
		lasterror = MTX_ERR_LOCAL;
		return 1;
	}

	return 0;
}

static mtx_listentry_t *get_rooms(mtx_session_t *session, mtx_room_context_t context)
{
	switch (context) {
	case MTX_ROOM_CONTEXT_JOIN:
		return &session->joinedrooms;
	case MTX_ROOM_CONTEXT_INVITE:
		return &session->invitedrooms;
	case MTX_ROOM_CONTEXT_LEAVE:
		return &session->leftrooms;
	default:
		assert(0);
	}
}
static void compute_room_state(mtx_listentry_t *rooms)
{
	for (mtx_listentry_t *e = rooms->next; e != rooms; e = e->next) {
		mtx_room_t *r = mtx_list_entry_content(e, mtx_room_t, entry);
		compute_room_state_from_history(r);
	}
}
void mtx_roomlist_init(mtx_listentry_t *rooms)
{
	mtx_list_init(rooms);
}
void mtx_roomlist_free(mtx_listentry_t *rooms)
{
	if (!rooms)
		return;

	mtx_list_free(rooms, mtx_room_t, entry, free_room);
}
int mtx_roomlist_update(mtx_session_t *session, mtx_listentry_t *_rooms, mtx_room_context_t context)
{
	mtx_listentry_t *rooms = get_rooms(session, context);

	compute_room_state(rooms);

	mtx_list_free(_rooms, mtx_room_t, entry, free_room);

	mtx_listentry_t *newrooms = _rooms;
	mtx_list_dup(newrooms, rooms, mtx_room_t, entry, dup_room);
	if (!newrooms)
		return 1;

	return 0;
}
int mtx_has_dirty_rooms(mtx_session_t *session, mtx_room_context_t context)
{
	mtx_listentry_t *rooms = get_rooms(session, context);

	for (mtx_listentry_t *e = rooms->next; e != rooms; e = e->next) {
		mtx_room_t *r = mtx_list_entry_content(e, mtx_room_t, entry);
		if (r->dirty)
			return 1;
	}
	return 0;
}

int mtx_sync_keys(mtx_session_t *session, int timeout)
{
	assert(session->accesstoken);

	if (query_keys(session, NULL, timeout))
		return 1;

	return 0;
}

static void print_device(device_t *dev)
{
	printf("id: %s\n", dev->id);

	if (dev->identkeys) {
		printf("identkeys:\n");
		printf("%s\n", json_object_to_json_string_ext(dev->identkeys,
					JSON_C_TO_STRING_PRETTY));
	}

	if (dev->otkeys) {
		printf("otkeys:\n");
		printf("%s\n", json_object_to_json_string_ext(dev->otkeys,
					JSON_C_TO_STRING_PRETTY));
	}

	if (dev->algorithms) {
		printf("algorithms:\n");
		for (size_t i = 0; dev->algorithms[i]; ++i) {
			printf("\t%s", dev->algorithms[i]);
		}
	}

	if (dev->displayname)
		printf("displayname: %s\n", dev->displayname);
}
void mtx_print_devices(mtx_session_t *session)
{
	printf("------- device -------\n");
	print_device(session->device);

	printf("------- other devices -------\n");
	mtx_list_foreach(&session->devices, device_t, entry, dev) {
		print_device(dev);
	}
}
