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

#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <stddef.h>

#include <json-c/json_types.h>
#include <olm/olm.h>

#include "mtx/devices.h"

typedef struct {
	mtx_listentry_t entry;

	char *algorithm;
	char *id;
	char *key;
} one_time_key_t;

struct megolm_session_t;
typedef struct megolm_session_t megolm_session_t;

int sign_json(OlmAccount *account, json_object *obj, const char *userid, const char *keyident);
const char *get_signature(const json_object *obj, const char *userid, const char *devid);
int verify_signature(json_object *obj, char *signature, const char *key);

void free_one_time_key(one_time_key_t *otkey);
int create_device_keys(OlmAccount *account, char **signkey, char **identkey);
int create_one_time_keys(OlmAccount *account, mtx_listentry_t *otkeys);

one_time_key_t *parse_one_time_key(const char *keyinfo, json_object *_key);

void free_olm_account(OlmAccount *acc);
OlmAccount *create_olm_account(void);

//megolm_session_t *create_outbound_megolm_session(unsigned long rotperiod, unsigned long rotmsgnum);
void free_megolm_session(megolm_session_t *session);
int update_megolm_session(megolm_session_t **session);

//static json_object *encrypt_event_megolm(megolm_session_t *session, device_t *dev,
//		const char *roomid, const char *evtype, json_object *evcontent);

json_object *device_keys_format(const char *signkey,
		const char *identkey, const char *devid);
int update_device_keys(json_object *_keys, char **signkey, char **identkey);

int verify_one_time_key_object(const char *keyidentif, json_object *_key,
		const char *userid, const char *devid, const char *identkey);
one_time_key_t *parse_one_time_key(const char *keyidentif, json_object *_key);
int one_time_key_format(OlmAccount *account, const char *userid, const char *devid,
		one_time_key_t *key, char **_keyidentif, json_object **_obj);

#endif /* ENCRYPTION_H */
