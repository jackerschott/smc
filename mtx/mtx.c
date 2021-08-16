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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <curl/curl.h>
#include <json-c/json.h>

#include "mtx/mtx.h"
#include "mtx/encryption.h"
#include "mtx/state.h"
#include "lib/hjson.h"
#include "lib/util.h"

#define CURLERR(code) do { \
	fprintf(stderr, "%s:%d/%s: ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, "%s\n", curl_easy_strerror((code))); \
} while (0);

#define URL_BUFSIZE 1024U
#define RESP_READSIZE 4096U

#define SECOND (1000L * 1000L * 1000L)

CURL *handle;
const char *origin = "https://localhost:8080";

typedef struct {
	size_t size;
	size_t len;
	char *data;
} respbuf_t;

typedef struct {
	listentry_t entry;

	char *id;
	json_object *identkeys;
	json_object *otkeys;

	char *displayname;
} device_t;

typedef struct {
	listentry_t entry;

	char *owner;
	listentry_t devices;
} device_list_t;

struct mtx_session_t {
	char *hostname;
	char *accesstoken;
	device_t dev;
	char *userid;

	char *nextbatch;

	int otkeycount;
};

listentry_t *devices;

int mtx_last_code;
merror_t mtx_last_err;
char mtx_last_errmsg[API_ERRORMSG_BUFSIZE];

static char *get_homeserver(const char *userid)
{
	const char *c = strchr(userid, ':');
	assert(c && strlen(c) > 1);

	return strdup(c + 1);
}

static int get_response_error(const json_object *resp, int code, merror_t *merror, char *errormsg)
{
	int err;
	int merr;
	if ((err = json_get_object_as_enum_(resp, "errcode", &merr, MERROR_NUM, merrorstr)))
		return err;
	*merror = (merror_t)merr;

	json_object *obj;
	json_object_object_get_ex(resp, "error", &obj);
	strcpy(errormsg, json_object_get_string(obj));
	errormsg[0] = tolower(errormsg[0]);
	return 0;
}

static void free_session(mtx_session_t *session)
{
	free(session->nextbatch);

	free(session->userid);

	free(session->dev.id);
	free(session->dev.id);
	json_object_put(session->dev.identkeys);
	json_object_put(session->dev.otkeys);

	free(session->accesstoken);
	free(session->hostname);
	free(session);
}
static int is_valid_session(mtx_session_t *session)
{
	return session->hostname && session->accesstoken && session->devid && session->userid;
}

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
static int api_call(const char *hostname, const char *request, const char *target, const char *urlparams,
		json_object *data, int *code, json_object **response)
{
	if (request)
		curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, request);

	char url[URL_BUFSIZE];
	strcpy(url, "https://");
	strcpy(url, hostname);
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
		return -1;
	}
	respbuf_t respbuf;
	respbuf.size = RESP_READSIZE;
	respbuf.len = 0;
	respbuf.data = respdata;
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &respbuf);

	CURLcode err = curl_easy_perform(handle);
	if (err) {
		CURLERR(err);
		curl_slist_free_all(header);
		free(respbuf.data);
		return -1;
	}
	curl_slist_free_all(header);
	respbuf.data[respbuf.len] = 0;

	int c;
	curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &c);

	json_object *resp = json_tokener_parse(respbuf.data);
	if (!resp) {
		fprintf(stderr, "%s: could not parse response", __func__);
		free(respbuf.data);
		return -1;
	}
	free(respbuf.data);
	*response = resp;
	*code = c;

	mtx_last_code = c;
	if (c == 200) {
		mtx_last_err = M_SUCCESS;
		strcpy(mtx_last_errmsg, "Success");
		return 0;
	} else {
		if (get_response_error(resp, c, &mtx_last_err, mtx_last_errmsg))
			return -1;
		return 1;
	} 
}
static int login(mtx_session_t *session, const char *username, const char *pass)
{
	json_object *data = json_object_new_object();
	if (!data)
		return -1;

	if (json_object_add_string_(data, "type", "m.login.password")
			|| json_object_add_string_(data, "user", username)
			|| json_object_add_string_(data, "password", pass)) {
		json_object_put(data);
		return -1;
	}

	//if (*devid && json_object_add_string_(data, "device_id", *devid)) {
	//	json_object_put(data);
	//	return 1;
	//}

	int code;
	json_object *resp;
	int err = api_call(session->hostname, "POST", "/_matrix/client/r0/login", NULL, data, &code, &resp);
	if (err) {
		json_object_put(data);
		return err;
	}
	json_object_put(data);

	if (json_get_object_as_string_(resp, "user_id", &session->userid)
			|| json_get_object_as_string_(resp, "access_token", &session->accesstoken)
			|| json_get_object_as_string_(resp, "device_id", &session->devid)) {
		json_object_put(resp);
		return -1;
	}

	json_object_put(resp);
	return 0;
}
static int query_user_id(mtx_session_t *session, char **userid)
{
	int code;
	json_object *resp;
	int err = api_call(session->hostname, "GET", "/_matrix/client/r0/account/whoami",
			NULL, NULL, &code, &resp);
	if (err)
		return err;

	if (json_get_object_as_string_(resp, "user_id", &session->userid)) {
		json_object_put(resp);
		return -1;
	}

	json_object_put(resp);
	return 0;
}

/* end-to-end encryption */
int upload_keys(mtx_session_t *session)
{
	json_object *data = json_object_new_object();
	if (!data)
		return 1;

	json_object *devkeys = json_object_new_object();
	if (!devkeys)
		goto err_free_data;
	if (json_object_object_add(data, "device_keys", devkeys)) {
		json_object_put(devkeys);
		goto err_free_data;
	}
	if (json_object_add_string_(devkeys, "user_id", session->userid)
			|| json_object_add_string_(devkeys, "device_id", session->dev.id)
			|| json_object_add_string_array_(devkeys, "algorithms",
					ARRNUM(crypto_algorithms_msg), crypto_algorithms_msg)) {
		goto err_free_data;
	}

	json_object *keys = json_object_new_object();
	if (!keys)
		goto err_free_data;
	if (json_object_object_add(devkeys, "keys", keys)) {
		json_object_put(keys);
		goto err_free_data;
	}

	json_object_object_foreach(session->dev.identkeys, k, v) {
		char *keyid = malloc(strlen(k) + STRLEN(":") + strlen(session->dev.id) + 1);
		if (!keyid) {
			goto err_free_data;
		}
		strcpy(keyid, k);
		strcat(keyid, ":");
		strcat(keyid, session->dev.id);

		if (json_object_object_add(keys, keyid, v)) {
			goto err_free_data;
		}
	}

	if (sign_json(devkeys, session->userid, session->dev.id))
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

	int code;
	json_object *resp;
	int err = api_call(session->hostname, "POST", "/_matrix/client/r0/keys/upload",
			NULL, data, &code, &resp);
	if (err)
		goto err_free_data;
	json_object_put(data);

err_free_data:
	json_object_put(data);
	return 1;
}

mtx_session_t *mtx_create_session(const char *hostname, const char *username,
		const char *password, const char *accesstoken, const char *device_id)
{
	assert(hostname);
	assert((accesstoken && device_id) || (!accesstoken && !device_id));

	if (curl_global_init(CURL_GLOBAL_SSL))
		return NULL;

	handle = curl_easy_init();
	if (!handle) {
		curl_global_cleanup();
		return NULL;
	}

	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, hrecv);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0); /* for testing */
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0); /* for testing */
	//curl_easy_setopt(handle, CURLOPT_VERBOSE, 1); /* for testing */

	mtx_session_t *session = malloc(sizeof(*session));
	if (!session)
		goto err_cleanup_curl;
	memset(session, 0, sizeof(*session));

	char *host = strdup(hostname);
	if (!host)
		goto err_free_session;
	session->hostname = host;

	char *token = NULL;
	if (accesstoken) {
		token = strdup(accesstoken);
		if (!token)
			goto err_free_session;
		session->accesstoken = token;
	}

	char *devid = NULL;
	if (device_id) {
		devid = strdup(device_id);
		if (!devid)
			goto err_free_session;
	}

	/* ensure accesstoken and device id, get user id */
	if (!session->accesstoken) {
		if (login(session, username, password))
			goto err_free_session;

		if (create_device_keys(&session->dev.identkeys))
			goto err_free_session;

		if (create_one_time_keys(&session->dev.otkeys))
			goto err_free_session;

		if (upload_keys(session)) {
			goto err_free_session;
		}
	} else {
		if (query_user_id(session, &session->userid))
			goto err_free_session;
	}

	/* get homeserver */
	char *hs = get_homeserver(session->userid);
	if (!hs)
		goto err_free_session;
	session->hostname = hs;

	return session;

err_free_session:
	free_session(session);
err_cleanup_curl:
	curl_easy_cleanup(handle);
	curl_global_cleanup();
	return NULL;
}
void mtx_cleanup_session(mtx_session_t *session)
{
	free_session(session);

	curl_easy_cleanup(handle);
	curl_global_cleanup();
}

int mtx_room_create(mtx_session_t *session, const char *clientid, const char *name,
		const char *alias, const char *topic, const char *preset, char **id)
{
	assert(is_valid_session(session));

	json_object *data = json_object_new_object();
	if (!data)
		return 1;

	if (json_object_add_string_(data, "name", name)
			|| json_object_add_string_(data, "room_alias_name", alias)
			|| json_object_add_string_(data, "topic", topic)
			|| json_object_add_string_(data, "preset", preset)) {
		json_object_put(data);
		return 1;
	}

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, session->accesstoken);

	int code;
	json_object *resp;
	int err = api_call(session->hostname, "POST", "/_matrix/client/r0/createRoom",
			urlparams, data, &code, &resp);
	if (err) {
		json_object_put(data);
		return err;
	}
	json_object_put(data);

	json_object *tmp;
	json_object_object_get_ex(resp, "room_id", &tmp);

	if (*id) {
		*id = strdup(json_object_get_string(tmp));
		if (!*id)
			return -1;
	}

	json_object_put(resp);
	return 0;
}
int mtx_room_leave(const char *id)
{
	assert(mtx_accesstoken);

	char target[URL_BUFSIZE];
	strcpy(target, "/_matrix/client/r0/rooms/");
	strcat(target, id);
	strcat(target, "/leave");

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, mtx_accesstoken);

	int code;
	json_object *resp;
	int err = api_call("POST", target, urlparams, NULL, &code, &resp);
	if (err)
		return err;

	json_object_put(resp);
	return 0;
}
int mtx_room_forget(const char *id)
{
	assert(mtx_accesstoken);

	char target[URL_BUFSIZE];
	strcpy(target, "/_matrix/client/r0/rooms/");
	strcat(target, id);
	strcat(target, "/forget");

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, mtx_accesstoken);

	int code;
	json_object *resp;
	int err = api_call("POST", target, urlparams, NULL, &code, &resp);
	if (err)
		return err;

	json_object_put(resp);
	return 0;
}
int mtx_room_list_joined(char ***roomids, size_t *nroomids)
{
	assert(mtx_accesstoken);

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, mtx_accesstoken);

	int code;
	json_object *resp;
	int err = api_call("GET", "/_matrix/client/r0/joined_rooms", urlparams, NULL, &code, &resp);
	if (err)
		return err;

	json_object *joinedroomsobj;
	json_object_object_get_ex(resp, "joined_rooms", &joinedroomsobj);
	size_t n = json_object_array_length(joinedroomsobj);

	char **ids = malloc(n * sizeof(*ids));
	if (!ids)
		return -1;
	memset(ids, 0, n * sizeof(*ids));
	for (size_t i = 0; i < n; ++i) {
		json_object *idobj = json_object_array_get_idx(joinedroomsobj, i);
		const char *s = json_object_get_string(idobj);

		char *id = malloc(strlen(s) + 1);
		if (!id)
			goto err_free_rooms;

		strcpy(id, s);
		ids[i] = id;
	}
	*roomids = ids;
	*nroomids = n;

	json_object_put(resp);
	return 0;

err_free_rooms:
	for (size_t i = 0; i < n; ++i) {
		free(ids[i]);
	}
	free(ids);
	return -1;
}

int mtx_invite(const char *roomid, const char *userid)
{
	json_object *data = json_object_new_object();
	if (!data)
		return -1;

	json_object *tmp = json_object_new_string(userid);
	if (!tmp) {
		json_object_put(data);
		return -1;
	}
	json_object_object_add(data, "user_id", tmp);

	char target[URL_BUFSIZE];
	strcpy(target, "/_matrix/client/r0/rooms/");
	strcat(target, roomid);
	strcat(target, "/invite");

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, mtx_accesstoken);

	int code;
	json_object *resp;
	int err = api_call("POST", target, urlparams, data, &code, &resp);
	if (err) {
		json_object_put(data);
		return err;
	}

	json_object_put(resp);
	json_object_put(data);
	return 0;
}
int mtx_join(const char *roomid)
{
	char target[URL_BUFSIZE];
	strcpy(target, "/_matrix/client/r0/rooms/");
	strcat(target, roomid);
	strcat(target, "/join");

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, mtx_accesstoken);

	int code;
	json_object *resp;
	int err = api_call("POST", target, urlparams, NULL, &code, &resp);
	if (err)
		return err;

	json_object_put(resp);
	return 0;
}

int sync_state(mtx_session_t *session, json_object **resp)
{
	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, session->accesstoken);
	
	if (session->nextbatch) {
		strcat(urlparams, "&since=");
		strcat(urlparams, session->nextbatch);
	}

	int code;
	int err = api_call(session->hostname, "GET", "/_matrix/client/r0/sync",
			urlparams, NULL, &code, resp);
	if (err)
		return err;

	free(session->nextbatch);
	if ((err = json_get_object_as_string_(*resp, "next_batch", &session->nextbatch))) {
		return err;
	}

	return 0;
}
int find_device(char *userid, char *devid, device_t **_dev)
{
	listentry_t *devs = NULL;
	for (listentry_t *e = devices->next; e != devices; e = e->next) {
		device_list_t *devlist = list_entry_content(e, device_list_t, entry);
		if (strcmp(devlist->owner, userid) == 0) {
			devs = &devlist->devices;
			break;
		}
	}
	if (!devs)
		return 1;

	device_t *dev = NULL;
	for (listentry_t *e = devs->next; e != devs; e = e->next) {
		device_t *d = list_entry_content(e, device_t, entry);
		if (strcmp(d->id, devid) == 0) {
			dev = d;
			break;
		}
	}
	if (!dev)
		return 1;

	*_dev = dev;
	return 0;
}
int add_device(char *userid, device_t *dev) {
	device_list_t *devlist = malloc(sizeof(*devlist));
	if (!devlist)
		return 1;

	char *owner = strdup(userid);
	if (!owner) {
		free(devlist);
		return 1;
	}
	devlist->owner = owner;

	list_add(&devlist->devices, &dev->entry);
}
int update_devices(char *owner, char *devid, json_object *devinfo)
{
	device_t *dev;
	if (find_device(owner, devid, &dev)) {
		dev = malloc(sizeof(*dev));
		if (!dev)
			return 1;
		memset(dev, 0, sizeof(*dev));

		if (add_device(owner, dev)) {
			free(dev);
			return 1;
		}

		char *id = strdup(devid);
		if (!id)
			return 1;
		dev->id = id;
	}

	json_object_object_get_ex(devinfo, "keys", &dev->identkeys);

	json_object *usigned;
	json_object_object_get_ex(devinfo, "unsigned", &usigned);
	if (usigned && json_get_object_as_string_(devinfo,
				"device_display_name", &dev->displayname) == -1)
		return 1;

	return 0;
}
int query_keys(mtx_session_t *session, listentry_t *devices, char *token, int timeout)
{
	json_object *data = json_object_new_object();
	if (!data)
		return -1;

	if (json_object_add_int_(data, "timeout", timeout)
			|| json_object_add_string_(data, "token", token)) {
		json_object_put(data);
		return -1;
	}

	json_object *devkeys;
	if (json_object_add_object_(data, "device_keys", &devkeys)) {
		json_object_put(data);
		return -1;
	}

	for (listentry_t *e = devices->next; e != devices; e = e->next) {
		device_list_t *devlist = list_entry_content(e, device_list_t, entry);

		json_object *_devlist;
		if (json_object_add_array_(data, devlist->owner, &_devlist)) {
			json_object_put(data);
			return -1;
		}

		listentry_t *devs = &devlist->devices;
		for (listentry_t *f = devs->next; f != devs; f = f->next) {
			device_t *dev = list_entry_content(devs, device_t, entry);

			if (json_array_add_string_(_devlist, dev->id)) {
				json_object_put(data);
				return -1;
			}
		}
	}

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, session->accesstoken);

	int code;
	json_object *resp;
	int err = api_call(session->hostname, "POST", "/_matrix/client/r0/keys/query",
			urlparams, data, &code, &resp);
	json_object_put(data);
	if (err)
		return err;

	// TODO: Handle failures

	json_object *qdevkeys;
	json_object_object_get_ex(resp, "device_keys", &qdevkeys);
	if (!qdevkeys) {
		json_object_put(resp);
		return -1;
	}


	json_object_object_foreach(qdevkeys, userid, devinfos) {
		json_object_object_foreach(devinfos, devid, devinfo) {
			if (update_devices(userid, devid, devinfo)) {
				json_object_put(resp);
				return -1;
			}
		}
	}

	json_object_put(resp);
	return 0;
}
int mtx_sync(mtx_session_t *session, json_object **resp)
{
	assert(is_valid_session(session));

	if (sync_state(session, resp))
		return 1;



	return 0;
}

int mtx_state(const char *roomid, const char *evtype,
		const char *statekey, json_object *event, char **evid)
{
	char target[URL_BUFSIZE];
	strcpy(target, "_matrix/client/r0/rooms/");
	strcat(target, roomid);
	strcat(target, "/state/");
	strcat(target, evtype);
	strcat(target, "/");
	strcat(target, statekey);

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, mtx_accesstoken);

	int code;
	json_object *resp;
	if (api_call("PUT", target, urlparams, event, &code, &resp))
		return 1;

	if (json_get_object_as_string_(resp, "event_id", evid)) {
		json_object_put(resp);
		return 1;
	}
	json_object_put(resp);
	return 0;
}
int mtx_send(const char *roomid, const char* evtype,  json_object *event, char **evid)
{
	assert(mtx_accesstoken);

	char txnid[TRANSACTION_ID_SIZE];
	if (generate_transaction_id(txnid))
		return 1;

	char target[URL_BUFSIZE];
	strcpy(target, "/_matrix/client/r0/rooms/");
	strcat(target, roomid);
	strcat(target, "/send/");
	strcat(target, evtype);
	strcat(target, "/");
	strcat(target, txnid);

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, mtx_accesstoken);

	int code;
	json_object *resp;
	int err = api_call("PUT", target, urlparams, event, &code, &resp);
	if (err)
		return err;

	if ((err = json_get_object_as_string_(resp, "event_id", evid))) {
		json_object_put(resp);
		return err;
	}
	json_object_put(resp);
	return 0;
}

int mtx_send_msg(const char *roomid, msg_t *msg, char **evid)
{
	json_object *event = json_object_new_object();
	if (!event)
		return 1;

	if (json_object_add_string_(event, "body", msg->body)
			|| json_object_add_enum_(event, "msgtype", msg->type, msg_type_str)) {
		json_object_put(event);
		return 1;
	}

	int err = mtx_send(roomid, "m.room.message", event, evid);
	json_object_put(event);
	return err;
}


int mtx_query_keys(int timeout)
{
	json_object *data = json_object_new_object();
	if (!data)
		return 1;

	return 0;
}

int mtx_room_enable_encryption(const char *roomid)
{
	json_object *event = json_object_new_object();
	if (!event)
		return 1;

	if (json_object_add_string_(event, "algorithm", "m.megolm.v1.aes-sha2")) {
		json_object_put(event);
		return 1;
	}

	char *evid;
	if (mtx_state(roomid, "m.room.encryption", "", event, &evid)) {
		json_object_put(event);
		return 1;
	}
	json_object_put(event);
	return 0;
}
