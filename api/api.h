#ifndef API_H
#define API_H

#include "json-c/json.h"
#include "api/state.h"

#define API_PARAM_BUFSIZE 2048U

#define API_MERROR_BUFSIZE 40U
#define API_ERRORMSG_BUFSIZE 256U
typedef enum {
	M_SUCCESS, /* not part of the spec */
	M_FORBIDDEN,
	M_UNKNOWN_TOKEN, \
	M_MISSING_TOKEN,
	M_BAD_JSON,
	M_NOT_JSON,
	M_NOT_FOUND,
	M_LIMIT_EXCEEDED,
	M_UNKNOWN,
	M_UNRECOGNIZED,
	M_UNAUTHORIZED,
	M_USER_DEACTIVATED,
	M_USER_IN_USE,
	M_INVALID_USERNAME,
	M_ROOM_IN_USE,
	M_INVALID_ROOM_STATE,
	M_THREEPID_IN_USE,
	M_THREEPID_NOT_FOUND,
	M_THREEPID_AUTH_FAILED,
	M_THREEPID_DENIED,
	M_SERVER_NOT_TRUSTED,
	M_UNSUPPORTED_ROOM_VERSION,
	M_INCOMPATIBLE_ROOM_VERSION,
	M_BAD_STATE,
	M_GUEST_ACCESS_FORBIDDEN,
	M_CAPTCHA_NEEDED,
	M_CAPTCHA_INVALID,
	M_MISSING_PARAM,
	M_INVALID_PARAM,
	M_TOO_LARGE,
	M_EXCLUSIVE,
	M_RESOURCE_LIMIT_EXCEEDED,
	M_CANNOT_LEAVE_SERVER_NOTICE_ROOM,
	MERROR_NUM,
} merror_t;
static const char *merrorstr[] = {
	"M_SUCCESS",
	"M_FORBIDDEN",
	"M_UNKNOWN_TOKEN",
	"M_MISSING_TOKEN",
	"M_BAD_JSON",
	"M_NOT_JSON",
	"M_NOT_FOUND",
	"M_LIMIT_EXCEEDED",
	"M_UNKNOWN",
	"M_UNRECOGNIZED",
	"M_UNAUTHORIZED",
	"M_USER_DEACTIVATED",
	"M_USER_IN_USE",
	"M_INVALID_USERNAME",
	"M_ROOM_IN_USE",
	"M_INVALID_ROOM_STATE",
	"M_THREEPID_IN_USE",
	"M_THREEPID_NOT_FOUND",
	"M_THREEPID_AUTH_FAILED",
	"M_THREEPID_DENIED",
	"M_SERVER_NOT_TRUSTED",
	"M_UNSUPPORTED_ROOM_VERSION",
	"M_INCOMPATIBLE_ROOM_VERSION",
	"M_BAD_STATE",
	"M_GUEST_ACCESS_FORBIDDEN",
	"M_CAPTCHA_NEEDED",
	"M_CAPTCHA_INVALID",
	"M_MISSING_PARAM",
	"M_INVALID_PARAM",
	"M_TOO_LARGE",
	"M_EXCLUSIVE",
	"M_RESOURCE_LIMIT_EXCEEDED",
	"M_CANNOT_LEAVE_SERVER_NOTICE_ROOM",
};

extern int api_last_code;
extern merror_t api_last_err;
extern char api_last_errmsg[API_ERRORMSG_BUFSIZE];

int api_init(void);
void api_cleanup(void);
int api_set_access_token(char *token);

int api_login(const char *username, const char *pass,
		char **id, char **token, char **homeserver, char **devid);
int api_room_create(const char *clientid, const char *name, const char *alias,
		const char *topic, const char *preset, char **id);
int api_room_leave(const char *id);
int api_room_forget(const char *id);
int api_room_list_joined(char ***joinedrooms, size_t *nrooms);
int api_sync(json_object **resp);

int api_send_msg(const char *roomid, msg_t *msg, char **evid);

int api_invite(const char *roomid, const char *userid);
int api_join(const char *roomid);

#endif /* API_H */
