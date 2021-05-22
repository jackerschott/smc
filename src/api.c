#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "api.h"
#include "smc.h"
#include "json-c/json.h"

#define CURLERR(code) do { \
	fprintf(stderr, "%s:%d/%s: ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, "%s\n", curl_easy_strerror((code))); \
} while (0);

#define URL_BUFSIZE 1024U
#define RESP_READSIZE 4096U

CURL *handle;
const char *baseurl = "https://localhost:8080";

struct respbuf_t {
	size_t size;
	size_t len;
	char *data;
};
typedef struct respbuf_t respbuf_t;

char *accesstoken;

int lastcode;
merror_t lasterr;
char lasterrmsg[ERRORMSG_BUFSIZE];

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


int api_init(void)
{
	CURLcode err = curl_global_init(CURL_GLOBAL_SSL);
	if (err)
		return 1;

	handle = curl_easy_init();
	if (!handle) {
		curl_global_cleanup();
		return 1;
	}

	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, hrecv);
	//curl_easy_setopt(handle, CURLOPT_READFUNCTION, hsend);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0); /* for testing */
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0); /* for testing */
	//curl_easy_setopt(handle, CURLOPT_VERBOSE, 1); /* for testing */

	return 0;
}
void api_cleanup(void)
{
	free(accesstoken);
	curl_easy_cleanup(handle);
	curl_global_cleanup();
}

int api_call(const char *request, const char *target, const char *urlparams,
		json_object *data, int *code, json_object **response)
{
	if (request)
		curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, request);

	char url[URL_BUFSIZE];
	strcpy(url, baseurl);
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

	int c;
	curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &c);

	json_object *resp = json_tokener_parse(respbuf.data);
	if (!response) {
		fprintf(stderr, "%s: could not parse response", __func__);
		free(respbuf.data);
		return -1;
	}
	free(respbuf.data);
	*response = resp;
	*code = c;

	lastcode = c;
	if (c == 200) {
		lasterr = M_SUCCESS;
		strcpy(lasterrmsg, "Success");
		return 0;
	} else {
		if (get_response_error(resp, c, &lasterr, lasterrmsg))
			return -1;
		return 1;
	} 
}

int api_set_access_token(char *token)
{
	char *s = strdup(token);
	if (!s)
		return 1;
	accesstoken = s;
	return 0;
}

int api_login(const char *username, const char *pass,
		char **id, char **token, char **homeserver, char **devid)
{
	json_object *data = json_object_new_object();
	json_object_object_add(data, "type", json_object_new_string("m.login.password"));
	json_object_object_add(data, "user", json_object_new_string(username));
	json_object_object_add(data, "password", json_object_new_string(pass));

	int code;
	json_object *resp = json_object_new_object();
	int err = api_call("POST", "/_matrix/client/r0/login", NULL, data, &code, &resp);
	if (err) {
		json_object_put(data);
		return err;
	}
	json_object_put(data);

	json_object *idobj, *tokenobj, *homeserverobj, *devidobj;
	json_object_object_get_ex(resp, "user_id", &idobj);
	json_object_object_get_ex(resp, "access_token", &tokenobj);
	json_object_object_get_ex(resp, "home_server", &homeserverobj);
	json_object_object_get_ex(resp, "device_id", &devidobj);
	if (id)
		*id = strdup(json_object_get_string(idobj));
	if (token)
		*token = strdup(json_object_get_string(tokenobj));
	if (homeserver)
		*homeserver = strdup(json_object_get_string(homeserverobj));
	if (devid)
		*devid = strdup(json_object_get_string(devidobj));

	if ((id && !*id) || (token && !*token)
			|| (homeserver && !*homeserver) || (devid && !*devid)) {
		free(*id);
		free(*token);
		free(*homeserver);
		free(*devid);
		return -1;
	}

	json_object_put(resp);
	return 0;
}
int api_room_create(const char *clientid, const char *name, const char *alias,
		const char *topic, const char *preset, char **id)
{
	assert(accesstoken);

	json_object *data = json_object_new_object();
	if (name)
		json_object_object_add(data, "name", json_object_new_string(name));
	if (alias)
		json_object_object_add(data, "room_alias_name", json_object_new_string(alias));
	if (topic)
		json_object_object_add(data, "topic", json_object_new_string(topic));
	if (preset)
		json_object_object_add(data, "preset", json_object_new_string(preset));

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, accesstoken);

	//printf("%s\n", json_object_to_json_string_ext(data, JSON_C_TO_STRING_PRETTY));
	//assert(0);

	int code;
	json_object *resp = json_object_new_object();
	int err = api_call("POST", "/_matrix/client/r0/createRoom", urlparams, data, &code, &resp);
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
int api_room_leave(const char *id)
{
	assert(accesstoken);

	char target[URL_BUFSIZE];
	strcpy(target, "/_matrix/client/r0/rooms/");
	strcat(target, id);
	strcat(target, "/leave");

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, accesstoken);

	int code;
	json_object *resp = json_object_new_object();
	int err = api_call("POST", target, urlparams, NULL, &code, &resp);
	if (err) {
		json_object_put(resp);
		return err;
	}

	json_object_put(resp);
	return 0;
}
int api_room_forget(const char *id)
{
	assert(accesstoken);

	char target[URL_BUFSIZE];
	strcpy(target, "/_matrix/client/r0/rooms/");
	strcat(target, id);
	strcat(target, "/forget");

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, accesstoken);

	int code;
	json_object *resp = json_object_new_object();
	int err = api_call("POST", target, urlparams, NULL, &code, &resp);
	if (err) {
		json_object_put(resp);
		return err;
	}

	json_object_put(resp);
	return 0;
}
int api_room_list_joined(char ***roomids, size_t *nroomids)
{
	assert(accesstoken);

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, accesstoken);

	int code;
	json_object *resp = json_object_new_object();
	int err = api_call("GET", "/_matrix/client/r0/joined_rooms", urlparams, NULL, &code, &resp);
	if (err) {
		json_object_put(resp);
		return err;
	}

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

int api_sync(state_t **state)
{
	assert(accesstoken);

	char urlparams[URL_BUFSIZE];
	strcpy(urlparams, "access_token=");
	strcat(urlparams, accesstoken);

	int code;
	json_object *resp = json_object_new_object();
	int err = api_call("GET", "/_matrix/client/r0/sync", urlparams, NULL, &code, &resp);
	if (err) {
		json_object_put(resp);
		return err;
	}

	if ((err = parse_state(resp, state))) {
		json_object_put(resp);
		return err;
	}
	json_object_put(resp);
	return 0;
}
