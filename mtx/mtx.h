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

#ifndef MTX_MTX_H
#define MTX_MTX_H

#include <stddef.h>

#include "lib/list.h"
#include "mtx/room.h"
#include "mtx/types.h"

int mtx_init(void);
void mtx_cleanup(void);

mtx_session_t *mtx_new_session(void);
void mtx_free_session(mtx_session_t *session);

void mtx_free_id(mtx_id_t *id);
mtx_id_t *mtx_create_id_user(const char *username);
mtx_id_t *mtx_create_id_third_party(const char *medium, const char *address);
mtx_id_t *mtx_create_id_phone(const char *country, const char *number);

int mtx_login_password(mtx_session_t *session, const char *hostname, mtx_id_t *id, const char *pass,
		const char *devid, const char *devname);
int mtx_login_token(mtx_session_t *session, const char *hostname, mtx_id_t *id, const char *token,
		const char *devid, const char *devname);
int mtx_recall_past_session(mtx_session_t *session, const char *hostname,
		const char *accesstoken, const char *devid);
const char *mtx_accesstoken(mtx_session_t *session);
const char *mtx_device_id(mtx_session_t *session);

int mtx_sync(mtx_session_t *session, int timeout);

void mtx_roomlist_init(mtx_listentry_t *rooms);
void mtx_roomlist_free(mtx_listentry_t *rooms);
int mtx_roomlist_update(mtx_session_t *session,
		mtx_listentry_t *_rooms, mtx_room_context_t context);
int mtx_has_dirty_rooms(mtx_session_t *session, mtx_room_context_t context);



#endif /* MTX_MTX_H */
