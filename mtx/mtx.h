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

#ifndef MTX_MTX_BLA_H
#define MTX_MTX_BLA_H

#include "lib/list.h"

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
static const char *mtx_api_error_strs[] = {
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

typedef struct roomlist_t roomlist_t;

struct _room_t;
typedef struct _room_t room_t;

struct mtx_session_t;
typedef struct mtx_session_t mtx_session_t;

int mtx_init(void);
void mtx_cleanup(void);

mtx_session_t *mtx_new_session(void);
void mtx_free_session(mtx_session_t *session);

typedef enum {
	MTX_ID_USER,
	MTX_ID_THIRD_PARTY,
	MTX_ID_PHONE,
} mtx_id_type_t;
typedef union {
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
} mtx_id_t;
int mtx_login_password(mtx_session_t *session, const char *hostname, mtx_id_t id, const char *pass,
		const char *devid, const char *devname);
int mtx_login_token(mtx_session_t *session, const char *hostname, mtx_id_t id, const char *token,
		const char *devid, const char *devname);
int mtx_past_session(mtx_session_t *session, const char *hostname,
		const char *accesstoken, const char *devid);

int mtx_sync(mtx_session_t *session, int timeout);
room_t *mtx_get_next_room(roomlist_t *rooms);
roomlist_t *mtx_get_joined_rooms(mtx_session_t *session);
roomlist_t *mtx_get_invited_rooms(mtx_session_t *session);
roomlist_t *mtx_get_left_rooms(mtx_session_t *session);
const char *mtx_room_get_id(room_t *r);
const char *mtx_room_get_name(room_t *r);
const char *mtx_room_get_topic(room_t *r);

#endif /* MTX_MTX_H */
