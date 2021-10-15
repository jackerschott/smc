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
#include <stdarg.h>
#include <string.h>

#include <curl/curl.h>
#include <json-c/json.h>

#include "lib/hjson.h"
#include "lib/list.h"
#include "mtx/devices.h"
#include "mtx/encryption.h"
#include "mtx/mtx.h"
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
	int published_devkeys;
	mtx_listentry_t devices;
	OlmAccount *olmaccount;
	OlmUtility *olmutil;
	megolm_session_t *megolmsession;

	char *nextbatch;
	mtx_listentry_t joinedrooms;
	mtx_listentry_t invitedrooms;
	mtx_listentry_t leftrooms;
};

typedef enum {
	ACCOUNT_USER,
	ACCOUNT_GUEST,
} account_type_t;

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
	// curl_easy_setopt(handle, CURLOPT_VERBOSE, 2); /* for testing */
}
static int get_response_error(json_object *resp,
		int code, mtx_error_t *error, char *errormsg)
{
	int apierr;
	if (json_get_enum_(resp, "errcode", &apierr, MTX_ERR_NUM, mtx_api_error_strs))
		return 1;
	*error = ((mtx_error_t)apierr) + MTX_ERR_M_UNKNOWN;

	const char *msg;
	if (json_get_string_(resp, "error", &msg))
		return 1;
	strcpy(errormsg, "api error: ");
	strcat(errormsg, msg);
	errormsg[0] = tolower(errormsg[0]);

	return 0;
}
static int api_call(const char *hostname, const char *request, const char *target,
		const char *urlparams, json_object *data, int *code, json_object **response)
{
	reset_handle(handle);

	if (request)
		curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, request);

	char url[URL_BUFSIZE];
	strcpy(url, "http://");
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
		lasterror = MTX_ERR_CONNECTION;
		return 1;
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
		if (get_response_error(resp, c, &lasterror, last_api_error_msg)) {
			lasterror = MTX_ERR_SUCCESS;
			return 0;
		}
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
	id[TRANSACTION_ID_SIZE] = 0;
	return 0;
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
	mtx_list_free(&session->joinedrooms, mtx_room_t, entry, mtx_free_room);
	mtx_list_free(&session->invitedrooms, mtx_room_t, entry, mtx_free_room);
	mtx_list_free(&session->leftrooms, mtx_room_t, entry, mtx_free_room);

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
	} else if (lasterror == MTX_ERR_CONNECTION) {
		msg = strdup("connection error");
	} else if (lasterror == MTX_ERR_SUCCESS) {
		msg = strdup("success");
	} else {
		assert(0);
	}
	if (!msg)
		return NULL;
	
	return msg;
}

static const char *get_homeserver(const char *userid)
{
	const char *c = strchr(userid, ':');
	assert(c && strlen(c) > 1);

	return c + 1;
}
static void free_register_stage(mtx_register_stage_t *stage)
{
	if (!stage)
		return;

	free(stage->type);
	free(stage);
}
static mtx_register_stage_t *dup_register_stage(mtx_register_stage_t *_stage)
{
	mtx_register_stage_t *stage = malloc(sizeof(*stage));
	if (!stage)
		return NULL;
	memset(stage, 0, sizeof(*stage));

	if (strrpl(&stage->type, _stage->type))
		return NULL;

	stage->credentials = NULL;
	if (_stage->credentials
			&& json_object_deep_copy(_stage->credentials, &stage->credentials, NULL))
		return NULL;

	return stage;
}
void mtx_free_register_flow(mtx_register_flow_t *flow)
{
	if (!flow)
		return;

	mtx_list_free(&flow->stages, mtx_register_stage_t, entry, free_register_stage);
	free(flow);
}
mtx_register_flow_t *mtx_dup_register_flow(mtx_register_flow_t *_flow)
{
	mtx_register_flow_t *flow = malloc(sizeof(*flow));
	if (!flow)
		return NULL;
	memset(flow, 0, sizeof(*flow));

	mtx_listentry_t *stages = &flow->stages;
	mtx_list_dup(stages, &_flow->stages, mtx_register_stage_t, entry, dup_register_stage);
	if (!stages)
		return NULL;

	return flow;
}
static mtx_register_stage_t *get_register_stage(const char *type, json_object *params)
{
	mtx_register_stage_t *stage = malloc(sizeof(*stage));
	if (!stage)
		return NULL;
	memset(stage, 0, sizeof(*stage));

	if (strrpl(&stage->type, type))
		goto err_free_stage;

	json_object_object_get_ex(params, type, &stage->credentials);

	return stage;

err_free_stage:
	free_register_stage(stage);
	return NULL;
}
static int get_register_flows(json_object *resp, mtx_listentry_t *flows, char **sessionkey)
{
	mtx_list_init(flows);

	json_object *_flows;
	assert(json_object_object_get_ex(resp, "flows", &_flows));
	assert(json_object_is_type(_flows, json_type_array));

	size_t nflows = json_object_array_length(_flows);
	for (size_t i = 0; i < nflows; ++i) {
		json_object *_flow = json_object_array_get_idx(_flows, i);

		mtx_register_flow_t *flow = malloc(sizeof(*flow));
		if (!flow)
			goto err_free_flows;
		memset(flow, 0, sizeof(*flow));
		mtx_list_init(&flow->stages);

		json_object *_stages;
		assert(json_object_object_get_ex(_flow, "stages", &_stages));
		assert(json_object_is_type(_stages, json_type_array));

		json_object *params;
		size_t nstages = json_object_array_length(_stages);
		for (size_t j = 0; j < nstages; ++j) {
			json_object *_stage = json_object_array_get_idx(_stages, j);
			assert(json_object_is_type(_stage, json_type_string));
			const char *type = json_object_get_string(_stage);

			assert(json_object_object_get_ex(resp, "params", &params));

			mtx_register_stage_t *stage = get_register_stage(type, params);
			if (!stage) {
				mtx_free_register_flow(flow);
				goto err_free_flows;
			}
			mtx_list_add(&flow->stages, &stage->entry);
		}

		mtx_list_add(flows, &flow->entry);
	}

	if (json_rpl_string_(resp, "session", sessionkey))
		goto err_free_flows;

	return 0;

err_free_flows:
	mtx_list_free(flows, mtx_register_flow_t, entry, mtx_free_register_flow);
	return 1;
}
static json_object *create_auth(mtx_register_stage_t *stage, const char *sessionkey)
{
	json_object *auth = json_object_new_object();
	if (!auth)
		return NULL;

	if (json_add_string_(auth, "type", stage->type))
		goto err_free_auth;

	if (sessionkey && json_add_string_(auth, "session", sessionkey))
		goto err_free_auth;

	if (stage->credentials) {
		json_object_object_foreach(stage->credentials, k, v) {
			assert(json_object_is_type(v, json_type_string));

			if (json_add_string_(auth, k, json_object_get_string(v)))
				goto err_free_auth;
		}
	}

	return auth;

err_free_auth:
	json_object_put(auth);
	return NULL;
}
static int register_account(mtx_session_t *session, const char *hostname, const account_type_t type,
		const char *username, const char *pass, char **devid, const char *devname,
		const int login, const mtx_register_flow_t *flow, mtx_listentry_t *flows,
		char **sessionkey)
{
	assert(!session->accesstoken);
	assert(username);

	json_object *data = json_object_new_object();
	if (!data)
		goto err_local;

	if (json_add_string_(data, "username", username)) {
		json_object_put(data);
		goto err_local;
	}
	if (pass && json_add_string_(data, "password", pass)) {
		json_object_put(data);
		goto err_local;
	}
	if (*devid && json_add_string_(data, "device_id", *devid)) {
		json_object_put(data);
		goto err_local;
	}
	if (devname && json_add_string_(data, "initial_device_display_name", devname)) {
		json_object_put(data);
		goto err_local;
	}
	if (json_add_bool_(data, "inhibit_login", !login)) {
		json_object_put(data);
		goto err_local;
	}

	if (flow) {
		mtx_register_stage_t *stage;
		mtx_list_entry_content_at(&flow->stages, mtx_register_stage_t, entry, 0, &stage);

		json_object *auth = create_auth(stage, *sessionkey);
		if (json_object_object_add(data, "auth", auth)) {
			json_object_put(data);
			goto err_local;
		}
	}

	int code;
	json_object *resp;
	if (api_call(hostname, "POST", "/_matrix/client/r0/register", NULL, data, &code, &resp)) {
		json_object_put(data);
		return 1;
	}
	json_object_put(data);

	if (code == 401) {
		if (get_register_flows(resp, flows, sessionkey)) {
			json_object_put(resp);
			goto err_local;
		}

		json_object_put(resp);
		return 0;
	}
	mtx_list_init(flows);
	free(*sessionkey);

	if (json_rpl_string_(resp, "user_id", &session->userid)) {
		json_object_put(resp);
		goto err_local;
	}

	const char *hs = get_homeserver(session->userid);
	if (!hs) {
		json_object_put(resp);
		goto err_local;
	}
	if (strrpl(&session->hostname, hs)) {
		json_object_put(resp); 
		goto err_local;
	}

	if (login) {
		char *_devid = NULL;
		if (json_rpl_string_(resp, "access_token", &session->accesstoken)
				|| json_rpl_string_(resp, "device_id", &_devid)) {
			json_object_put(resp);
			goto err_local;
		}
		*devid = _devid;

		OlmAccount *acc = create_olm_account();
		if (!acc) {
			json_object_put(resp);
			goto err_local;
		}
		free_olm_account(session->olmaccount);
		session->olmaccount = acc;

		device_t *device = create_own_device(session->olmaccount, _devid);
		if (!device) {
			json_object_put(resp);
			goto err_local;
		}
		free_device(session->device);
		session->device = device;
	}
	json_object_put(resp);

	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
int mtx_register_user(mtx_session_t *session, const char *hostname, const char *username,
		const char *pass, char **devid, const char *devname, const int login,
		const mtx_register_flow_t *flow, mtx_listentry_t *flows, char **sessionkey)
{
	return register_account(session, hostname, ACCOUNT_USER,
			username, pass, devid, devname, login, flow, flows, sessionkey);
}
int mtx_register_guest(mtx_session_t *session, const char *hostname, const char *username,
		const char *pass, char **devid, const char *devname, const int login,
		const mtx_register_flow_t *flow, mtx_listentry_t *flows, char **sessionkey)
{
	return register_account(session, hostname, ACCOUNT_GUEST,
			username, pass, devid, devname, login, flow, flows, sessionkey);
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
static int login(mtx_session_t *session, const char *hostname, const char *typestr, mtx_id_t *id,
		const char *secret, const char *_devid, const char *devname)
{
	assert(!session->accesstoken);

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

	const char *devid;
	if (json_rpl_string_(resp, "access_token", &session->accesstoken)
			|| json_rpl_string_(resp, "user_id", &session->userid)
			|| json_get_string_(resp, "device_id", &devid)) {
		json_object_put(resp);
		goto err_local;
	}

	const char *hs = get_homeserver(session->userid);
	if (!hs)
		goto err_local;
	if (strrpl(&session->hostname, hs))
		goto err_local;

	OlmAccount *acc = create_olm_account();
	if (!acc)
		goto err_local;
	free(session->olmaccount);
	session->olmaccount = acc;

	device_t *device = create_own_device(session->olmaccount, devid);
	if (!device)
		goto err_local;
	free_device(session->device);
	session->device = device;

	json_object_put(resp);
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
const char *mtx_hostname(mtx_session_t *session)
{
	return session->hostname;
}

int mtx_logout(mtx_session_t *session)
{
	char urlparams[URL_BUFSIZE];
	sprintf(urlparams, "access_token=%s", session->accesstoken);

	int code;
	json_object *resp;
	if (api_call(session->hostname, "POST", "/_matrix/client/r0/logout",
				urlparams, NULL, &code, &resp))
		return 1;
	json_object_put(resp); // empty

	if (strrpl(&session->accesstoken, NULL))
		return 1;
	return 0;
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
	if (json_rpl_string_(resp, "user_id", userid)) {
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

	const char *hs = get_homeserver(userid);
	if (!hs)
		goto err_local;

	if (strrpl(&session->hostname, hs))
		goto err_local;

	if (strrpl(&session->accesstoken, accesstoken))
		goto err_local;

	if (strrpl(&session->userid, userid))
		goto err_local;
	free(userid);

	OlmAccount *acc = create_olm_account();
	if (!acc)
		goto err_local;
	session->olmaccount = acc;

	device_t *device = create_own_device(session->olmaccount, devid);
	if (!device)
		goto err_local;
	session->device = device;

	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}

static int upload_keys(mtx_session_t *session)
{
	json_object *data = json_object_new_object();
	if (!data)
		goto err_local;

	/* device keys */
	if (!session->published_devkeys) {
		json_object *devkeys = json_object_new_object();
		if (!devkeys)
			goto err_free_data;
		if (!json_object_object_add(data, "device_keys", devkeys)) {
			json_object_put(devkeys);
			goto err_free_data;
		}
		if (json_add_string_(devkeys, "user_id", session->userid)
				|| json_add_string_(devkeys, "device_id", session->device->id)
				|| json_add_string_array_(devkeys, "algorithms", mtx_crypt_algorithm_strs)) {
			goto err_free_data;
		}

		json_object *keys = device_keys_format(session->device->signkey,
				session->device->identkey, session->device->id);
		if (!keys)
			goto err_free_data;

		if (!json_object_object_add(devkeys, "keys", keys)) {
			json_object_put(keys);
			goto err_free_data;
		}

		if (sign_json(session->olmaccount, devkeys, session->userid, session->device->id))
			goto err_free_data;

		session->published_devkeys = 1;
	}

	/* one time keys */
	json_object *otkeys;
	if (json_add_object_(data, "one_time_keys", &otkeys)) {
		json_object_put(otkeys);
		goto err_free_data;
	}

	mtx_list_foreach(&session->device->otkeys, one_time_key_t, entry, otkey) {
		json_object *key;
		char *keyidentif;
		if (one_time_key_format(session->olmaccount, session->userid,
				session->device->id, otkey, &keyidentif, &key))
			goto err_free_data;

		if (json_object_object_add(data, keyidentif, key))
			goto err_free_data;
	}

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

	if (timeout != -1 && json_add_int_(data, "timeout", timeout)) {
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
		device_list_t *devlist = find_device_list(&session->devices, userid);
		assert(devlist);

		json_object_object_foreach(devinfos, devid, devinfo) {
			device_t *dev = find_device(devlist, devid);

			int err = verify_device_object(devinfo, userid, devid, dev);
			if (err == -1) {
				goto err_local;
			} else if (err) {
				continue;
			}

			if (!dev) {
				device_t *_dev = malloc(sizeof(*dev));
				if (!_dev)
					goto err_local;
				memset(_dev, 0, sizeof(*_dev));

				dev = _dev;
				mtx_list_add(&devlist->devices, &dev->entry);
			}

			if (update_device(dev, devinfo)) {
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
static int claim_one_time_keys(mtx_session_t *session, int timeout, json_object *keys)
{
	json_object *data = json_object_new_object();
	if (!data)
		goto err_local;

	if (timeout != -1 && json_add_int_(data, "timeout", timeout))
		goto err_free_data;

	json_object *_keys = NULL;
	if (json_object_deep_copy(keys, &_keys, NULL))
		goto err_free_data;

	if (json_object_object_add(data, "one_time_keys", _keys)) {
		json_object_put(_keys);
		goto err_free_data;
	}

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, session->accesstoken);

	int code;
	json_object *resp;
	if (api_call(session->hostname, "POST", "/_matrix/client/r0/keys/claim",
				urlparams, data, &code, &resp)) {
		json_object_put(data);
		return 1;
	}
	json_object_put(data);

	// TODO: handle failures

	json_object *otkeys;
	if (!json_object_object_get_ex(resp, "one_time_keys", &otkeys)) {
		json_object_put(resp);
		goto err_local;
	}

	json_object_object_foreach(otkeys, userid, v1) {
		device_list_t *devlist = find_device_list(&session->devices, userid);
		assert(devlist);

		json_object_object_foreach(v1, devid, v2) {
			device_t *dev = find_device(devlist, devid);
			assert(dev);

			json_object_object_foreach(v2, keyidentif, key) {
				int err = verify_one_time_key_object(keyidentif, key,
						userid, dev->id, dev->identkey);
				if (err == -1) {
					json_object_put(resp);
					goto err_local;
				} else if (err) {
					continue;
				}

				one_time_key_t *otkey = parse_one_time_key(keyidentif, key);
				if (!otkey) {
					json_object_put(resp);
					goto err_local;
				}

				mtx_list_add(&dev->otkeys, &otkey->entry);
			}
		}
	}

	json_object_put(resp);
	return 0;

err_free_data:
	json_object_put(data);
err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
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
	if (json_rpl_string_(resp, "next_batch", &nextbatch))
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

	json_object *todevice;
	if (json_object_object_get_ex(resp, "to_device", &todevice)) {
		mtx_listentry_t events;
		if (get_to_device(todevice, &events))
			goto err_local;

		if (apply_to_device_events(&events))
			goto err_local;
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
	json_object_put(resp);
	free(response);
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

	mtx_list_free(rooms, mtx_room_t, entry, mtx_free_room);
}
int mtx_roomlist_update(mtx_session_t *session, mtx_listentry_t *_rooms, mtx_room_context_t context)
{
	mtx_listentry_t *rooms = get_rooms(session, context);

	compute_room_state(rooms);

	mtx_list_free(_rooms, mtx_room_t, entry, mtx_free_room);

	mtx_listentry_t *newrooms = _rooms;
	mtx_list_dup(newrooms, rooms, mtx_room_t, entry, mtx_dup_room);
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

static int send_to_device_event(const mtx_session_t *session,
		const char *evtype, json_object *messages)
{
	json_object *data = json_object_new_object();
	if (!data)
		goto err_local;

	json_object *_messages = NULL;
	if (json_object_deep_copy(messages, &_messages, NULL)) {
		json_object_put(data);
		goto err_local;
	}

	if (json_add_object_(data, "messages", &_messages)) {
		json_object_put(_messages);
		json_object_put(data);
		goto err_local;
	}

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token");
	strcat(urlparams, session->accesstoken);

	char txnid[TRANSACTION_ID_SIZE + 1];
	if (generate_transaction_id(txnid)) {
		goto err_local;
	}

	char target[URL_BUFSIZE];
	sprintf(target, "/_matrix/client/r0/sendToDevice/%s/%s", evtype, txnid);

	int code;
	json_object *resp;
	if (api_call(session->hostname, "PUT", target, urlparams, data, &code, &resp)) {
		return 1;
	}

	json_object_put(resp);
	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
int mtx_send_key_request(mtx_session_t *session, char **userids, mtx_ev_room_key_request_t *request)
{
	json_object *messages = json_object_new_object();
	if (!messages)
		goto err_local;

	json_object *event = format_ev_room_key_request(request);
	if (!event)
		goto err_free_messages;

	for (size_t i = 0; userids[i]; ++i) {
		json_object *msgs;
		if (json_add_object_(messages, userids[i], &msgs))
			goto err_free_messages;

		if (json_add_object_(msgs, "*", &event))
			goto err_free_messages;
	}

	if (send_to_device_event(session, "m.room_key_request", messages))
		return 1;

	return 0;

err_free_messages:
	json_object_put(messages);
err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}

static int send_message_event(const mtx_session_t *session, const char *roomid,
		const char *evtype, json_object *event, char **eventid)
{
	char urlparams[URL_BUFSIZE];
	sprintf(urlparams, "access_token=%s", session->accesstoken);

	char txnid[TRANSACTION_ID_SIZE + 1];
	if (generate_transaction_id(txnid))
		goto err_local;

	char target[URL_BUFSIZE];
	char *_roomid = curl_easy_escape(handle, roomid, strlen(roomid));
	sprintf(target, "/_matrix/client/r0/rooms/%s/send/%s/%s", _roomid, evtype, txnid);
	free(_roomid);

	int code;
	json_object *resp;
	if (api_call(session->hostname, "PUT", target, urlparams, event, &code, &resp))
		return 1;

	*eventid = NULL;
	if (json_rpl_string_(resp, "event_id", eventid))
		goto err_local;

	json_object_put(resp);
	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
int mtx_send_message(mtx_session_t *session, const mtx_room_t *room,
		const mtx_ev_message_t *msg)
{
	assert(session->accesstoken);

	json_object *ev = format_ev_message(msg);
	if (!ev)
		goto err_local;

	char *eventid;
	//if (room->crypt.enabled) {
	//	if (!session->megolmsession) {
	//		megolm_session_t *ms = create_outbound_megolm_session(
	//				room->crypt.rotperiod, room->crypt.rotmsgnum);
	//		if (!ms)
	//			goto err_local;
	//		session->megolmsession = ms;
	//	}

	//	if (update_megolm_session(&session->megolmsession))
	//		goto err_local;

	//	json_object *content = ev;
	//	ev = encrypt_event_megolm(session->megolmsession, session->device,
	//			room->id, "m.room.message", content);
	//	json_object_put(content);
	//}

	if (send_message_event(session, room->id, "m.room.message", ev, &eventid))
		return 1;
	json_object_put(ev);

	free(eventid); /* probably useless */
	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}

static json_object *format_room_creation_info(const mtx_room_creation_info_t *createinfo)
{
	json_object *obj = json_object_new_object();
	if (!obj)
		return NULL;

	if (createinfo->visib != MTX_ROOM_VISIBILITY_NUM
			&& json_add_enum_(obj, "visibility",
				createinfo->visib, mtx_room_visibility_strs))
		goto err_free_obj;

	if (createinfo->alias && json_add_string_(obj, "room_alias_name", createinfo->alias))
		goto err_free_obj;

	if (createinfo->name && json_add_string_(obj, "name", createinfo->name))
		goto err_free_obj;

	if (createinfo->topic && json_add_string_(obj, "topic", createinfo->topic))
		goto err_free_obj;

	if (createinfo->invites && json_add_string_array_(obj, "invite", createinfo->invites))
		goto err_free_obj;

	// TODO: invite_3pid
	
	if (createinfo->version && json_add_string_(obj, "room_version", createinfo->version))
		goto err_free_obj;

	// TODO: creation_content

	// TODO: initial_state

	if (createinfo->preset != MTX_ROOM_PRESET_NUM &&
			json_add_enum_(obj, "preset", createinfo->preset, mtx_room_preset_strs))
		goto err_free_obj;

	if (createinfo->isdirect != -1 && json_add_bool_(obj, "is_direct", createinfo->isdirect))
		goto err_free_obj;

	// TODO: power_level_content_override

	return obj;

err_free_obj:
	json_object_put(obj);
	return NULL;
}
int mtx_create_room(const mtx_session_t *session,
		mtx_room_creation_info_t *createinfo, char **roomid)
{
	json_object *data = format_room_creation_info(createinfo);
	if (!data)
		goto err_local;

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, session->accesstoken);

	int code;
	json_object *resp;
	if (api_call(session->hostname, "POST", "/_matrix/client/r0/createRoom",
				urlparams, data, &code, &resp)) {
		json_object_put(data);
		return 1;
	}
	json_object_put(data);

	*roomid = NULL;
	if (json_rpl_string_(resp, "room_id", roomid)) {
		json_object_put(resp);
		goto err_local;
	}

	json_object_put(resp);
	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
int mtx_create_room_from_preset(const mtx_session_t *session,
		mtx_room_preset_t preset, char **roominfo)
{
	mtx_room_creation_info_t createinfo;
	memset(&createinfo, 0, sizeof(createinfo));
	createinfo.preset = preset;
	createinfo.visib = MTX_ROOM_VISIBILITY_NUM;
	createinfo.isdirect = -1;

	return mtx_create_room(session, &createinfo, roominfo);
}

int mtx_invite(const mtx_session_t *session, const char *roomid, const char *userid)
{
	json_object *data = json_object_new_object();
	if (!data)
		goto err_local;

	if (json_add_string_(data, "user_id", userid)) {
		json_object_put(data);
		goto err_local;
	}

	char urlparams[URL_BUFSIZE];
	sprintf(urlparams, "access_token=%s", session->accesstoken);

	char target[URL_BUFSIZE];
	char *_roomid = curl_easy_escape(handle, roomid, strlen(roomid));
	sprintf(target, "/_matrix/client/r0/rooms/%s/invite", _roomid);
	free(_roomid);

	int code;
	json_object *resp;
	if (api_call(session->hostname, "POST", target, urlparams, data, &code, &resp)) {
		json_object_put(data);
		return 1;
	}
	json_object_put(data);

	json_object_put(resp); // resp is empty
	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
int mtx_join(const mtx_session_t *session, const char *roomid)
{
	json_object *data = json_object_new_object();
	if (!data)
		goto err_local;

	// TODO: third_party_signed

	char urlparams[URL_BUFSIZE];
	sprintf(urlparams, "access_token=%s", session->accesstoken);

	char target[URL_BUFSIZE];
	sprintf(target, "/_matrix/client/r0/rooms/%s/join", roomid);

	int code;
	json_object *resp;
	if (api_call(session->hostname, "POST", target, urlparams, data, &code, &resp)) {
		json_object_put(data);
		return 1;
	}
	json_object_put(data);

	json_object_put(resp); // resp contains unneeded roomid
	return 0;

err_local:
	lasterror = MTX_ERR_LOCAL;
	return 1;
}
