#ifndef MTX_TYPES_H
#define MTX_TYPES_H

#include "lib/list.h"
#include "mtx/room.h"

struct mtx_session_t;
typedef struct mtx_session_t mtx_session_t;

typedef struct {
	mtx_listentry_t entry;

	mtx_listentry_t stages;
} mtx_register_flow_t;
typedef struct {
	mtx_listentry_t entry;

	char *type;
	json_object *credentials;
} mtx_register_stage_t;

union mtx_id_t;
typedef union mtx_id_t mtx_id_t;

struct mtx_sync_response_t;
typedef struct mtx_sync_response_t mtx_sync_response_t;

typedef enum {
	MTX_ROOM_PRESET_PRIVATE_CHAT,
	MTX_ROOM_PRESET_PUBLIC_CHAT,
	MTX_ROOM_PRESET_TRUSTED_PRIVATE_CHAT,
	MTX_ROOM_PRESET_NUM,
} mtx_room_preset_t;
static char *mtx_room_preset_strs[] = {
	"private_chat",
	"public_chat",
	"trusted_private_chat",
};
typedef struct  {
	mtx_room_preset_t preset;

	char *version;
	char *name;
	char *topic;
	char *alias;
	int isdirect;
	mtx_room_visibility_t visib;

	char **invites;
	// TODO: third party invites;

	mtx_ev_create_t *createcontent;
	mtx_ev_powerlevels_t powerlevelscontent;
	mtx_listentry_t initstate;
} mtx_room_creation_info_t;

typedef enum {
	MTX_ERR_SUCCESS,
	MTX_ERR_LOCAL,

	/* matrix api errors */
	MTX_ERR_M_FORBIDDEN,
	MTX_ERR_M_UNKNOWN_TOKEN, \
	MTX_ERR_M_MISSING_TOKEN,
	MTX_ERR_M_BAD_JSON,
	MTX_ERR_M_NOT_JSON,
	MTX_ERR_M_NOT_FOUND,
	MTX_ERR_M_LIMIT_EXCEEDED,
	MTX_ERR_M_UNKNOWN,
	MTX_ERR_M_UNRECOGNIZED,
	MTX_ERR_M_UNAUTHORIZED,
	MTX_ERR_M_USER_DEACTIVATED,
	MTX_ERR_M_USER_IN_USE,
	MTX_ERR_M_INVALID_USERNAME,
	MTX_ERR_M_ROOM_IN_USE,
	MTX_ERR_M_INVALID_ROOM_STATE,
	MTX_ERR_M_THREEPID_IN_USE,
	MTX_ERR_M_THREEPID_NOT_FOUND,
	MTX_ERR_M_THREEPID_AUTH_FAILED,
	MTX_ERR_M_THREEPID_DENIED,
	MTX_ERR_M_SERVER_NOT_TRUSTED,
	MTX_ERR_M_UNSUPPORTED_ROOM_VERSION,
	MTX_ERR_M_INCOMPATIBLE_ROOM_VERSION,
	MTX_ERR_M_BAD_STATE,
	MTX_ERR_M_GUEST_ACCESS_FORBIDDEN,
	MTX_ERR_M_CAPTCHA_NEEDED,
	MTX_ERR_M_CAPTCHA_INVALID,
	MTX_ERR_M_MISSING_PARAM,
	MTX_ERR_M_INVALID_PARAM,
	MTX_ERR_M_TOO_LARGE,
	MTX_ERR_M_EXCLUSIVE,
	MTX_ERR_M_RESOURCE_LIMIT_EXCEEDED,
	MTX_ERR_M_CANNOT_LEAVE_SERVER_NOTICE_ROOM,
	MTX_ERR_NUM,
} mtx_error_t;
static char *mtx_api_error_strs[] = {
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

#endif /* MTX_TYPES_H */
