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
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>

#include <json-c/json.h>

#include "mtx/encryption.h"
#include "mtx/state/room.h"
#include "lib/hjson.h"
#include "lib/util.h"

static const char *algorithm_signing = "ed25519";

struct megolm_session_t {
	OlmOutboundGroupSession *groupsession;

	unsigned long rotperiod;
	unsigned long rotmsgnum;

	unsigned long tpcreation;
};

OlmUtility *util = NULL;

static void string2byte(const char *s, uint8_t *b, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		b[i] = s[i];
	}
}
static void byte2string(const uint8_t *b, char *s, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		s[i] = b[i];
	}
}

static void split_key_identifier(const char *keyinfo,
		const char **algorithm, const char **id)
{
	char *c = strchr(keyinfo, ':');
	assert(c);
	*c = 0;

	if (algorithm)
		*algorithm = keyinfo;

	if (id)
		*id = c + 1;
}
static size_t get_key_identifier_length(const char *algorithm, const char *id)
{
	return strlen(algorithm) + STRLEN(":") + strlen(id);
}
static void get_key_identifier(const char *algorithm, const char *id, char *keyidentif)
{
	strcpy(keyidentif, algorithm);
	strcat(keyidentif, ":");
	strcat(keyidentif, id);
}

static const char *get_canonical_json_string(json_object *obj)
{
	return json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
}
static char *get_signature_target(json_object *obj)
{
	json_object *target = NULL;
	json_object_deep_copy(obj, &target, NULL);

	json_object_object_del(target, "unsigned");
	json_object_object_del(target, "signatures");

	char *s = strdup(get_canonical_json_string(target));
	json_object_put(target);
	return s;
}
int sign_json(OlmAccount *account, json_object *obj, const char *userid, const char *devid)
{
	size_t signlen = olm_account_signature_length(account);
	char signature[signlen];

	char *target = get_signature_target(obj);
	if (!target)
		return 1;

	if (olm_account_sign(account, target, strlen(target),
				signature, signlen) == olm_error()) {
		free(target);
		return 1;
	}
	free(target);

	json_object *signatures;
	if (json_add_object_(obj, "signatures", &signatures))
		return 1;

	json_object *sign;
	if (json_add_object_(obj, userid, &sign))
		return 1;

	char signkey[strlen(algorithm_signing) + STRLEN(":") + strlen(devid)];
	strcpy(signkey, algorithm_signing);
	strcat(signkey, ":");
	strcat(signkey, devid);

	if (json_add_string_(sign, signkey, signature))
		return 1;

	return 0;
}
const char *get_signature(const json_object *obj, const char *userid, const char *devid)
{
	json_object *signatures;
	if (json_object_object_get_ex(obj, "signatures", &signatures))
		return NULL;

	json_object_object_foreach(signatures, k, v) {
		if (strcmp(k, userid) != 0)
			continue;

		json_object_object_foreach(v, k2, v2) {
			const char *algorithm;
			const char *_devid;
			split_key_identifier(k2, &algorithm, &_devid);
			assert(strcmp(algorithm, "ed25519") == 0);

			if (strcmp(_devid, devid) != 0)
				continue;

			assert(json_object_is_type(v2, json_type_string));
			return json_object_get_string(v2);
		}
	}
	
	return NULL;
}
int verify_signature(json_object *obj, char *signature, const char *key)
{
	if (!util) {
		util = malloc(olm_utility_size());
		if (!util)
			return 1;
		util = olm_utility(util);
	}


	char *target = get_signature_target(obj);
	if (!target)
		return -1;

	if (olm_ed25519_verify(util, key, strlen(key), target,
				strlen(target), signature, strlen(signature)) == olm_error()) {
		if (strcmp(olm_utility_last_error(util), "BAD_MESSAGE_MAC") != 0)
			assert(0);
		return 1;
	}
	free(target);
	return 0;
}

void free_one_time_key(one_time_key_t *otkey)
{
	if (!otkey)
		return;

	free(otkey->algorithm);
	free(otkey->id);
	free(otkey->key);
	free(otkey);
}
int create_device_keys(OlmAccount *account, char **signkey, char **identkey)
{
	size_t len = olm_account_identity_keys_length(account);
	char *keys = malloc(len + 1);
	if (!keys)
		return 1;

	if (olm_account_identity_keys(account, keys, len) == olm_error()) {
		free(keys);
		return 1;
	}
	keys[len] = 0;

	json_object *_keys = json_tokener_parse(keys);
	if (!_keys) {
		free(keys);
		return 1;
	}
	free(keys);

	if (json_rpl_string_(_keys, "ed25519", signkey)) {
		json_object_put(_keys);
		return 1;
	}

	if (json_rpl_string_(_keys, "curve25519", identkey)) {
		json_object_put(_keys);
		return 1;
	}

	json_object_put(_keys);
	return 0;
}
int create_one_time_keys(OlmAccount *account, mtx_listentry_t *otkeys)
{
	size_t nkeys = olm_account_max_number_of_one_time_keys(account) / 2;

	size_t rdlen = olm_account_generate_one_time_keys_random_length(account, nkeys);
	void *random = malloc(rdlen);
	if (!random)
		return 1;

	if (getrandom_(random, rdlen)) {
		free(random);
		return 1;
	}

	if (olm_account_generate_one_time_keys(account, nkeys, random, rdlen) == olm_error()) {
		free(random);
		return 1;
	}
	free(random);

	size_t len = olm_account_one_time_keys_length(account);
	char keys[len + 1];
	if (olm_account_one_time_keys(account, keys, len) == olm_error())
		return 1;
	keys[len] = 0;

	json_object *_keys = json_tokener_parse(keys);
	if (!_keys)
		return 1;

	mtx_list_init(otkeys);
	json_object_object_foreach(_keys, k1, v1) {
		json_object_object_foreach(v1, k2, v2) {
			one_time_key_t *otkey = malloc(sizeof(*otkey));
			if (!otkey) {
				json_object_put(_keys);
				return 1;
			}
			memset(otkey, 0, sizeof(*otkey));

			if (strrpl(&otkey->algorithm, k1)) {
				json_object_put(_keys);
				return 1;
			}

			if (strrpl(&otkey->id, k2)) {
				json_object_put(_keys);
				return 1;
			}

			const char *key = json_object_get_string(v2);
			if (strrpl(&otkey->key, key)) {
				json_object_put(_keys);
				return 1;
			}

			mtx_list_add(otkeys, &otkey->entry);
		}
	}

	json_object_put(_keys);
	return 0;

err_free_otkeys:
	mtx_list_free(otkeys, one_time_key_t, entry, free_one_time_key);
	return 1;
}

json_object *device_keys_format(const char *signkey,
		const char *identkey, const char *devid)
{
	json_object *keys = json_object_new_object();
	if (!keys)
		return NULL;

	char signkeyidentif[get_key_identifier_length("ed25519:", devid) + 1];
	get_key_identifier("ed25519", devid, signkeyidentif);
	if (json_add_string_(keys, signkeyidentif, signkey)) {
		json_object_put(keys);
		return NULL;
	}

	char identkeyidentif[get_key_identifier_length("curve25519", devid) + 1];
	get_key_identifier("curve25519", devid, identkeyidentif);
	if (json_add_string_(keys, identkeyidentif, identkey)) {
		json_object_put(keys);
		return NULL;
	}
	
	return keys;
}
int update_device_keys(json_object *_keys, char **signkey, char **identkey)
{
	json_object_object_foreach(_keys, k, v) {
		const char *algorithm;
		const char *devid;
		split_key_identifier(k, &algorithm, &devid);

		const char *key = json_object_get_string(v);
		if (strcmp(algorithm, "ed25519") == 0) {
			if (signkey && strrpl(signkey, key))
				return 1;
		} else if (strcmp(algorithm, "curve25519") == 0) {
			if (identkey && strrpl(identkey, key))
				return 1;
		}
	}

	return 0;
}

int verify_one_time_key_object(const char *keyidentif, json_object *_key,
		const char *userid, const char *devid, const char *identkey)
{
	const char *algorithm;
	split_key_identifier(keyidentif, &algorithm, NULL);

	if (strcmp(algorithm, "ed25519") == 0 && strcmp(algorithm, "curve25519") == 0) {
		return 0;
	}
	assert(strcmp(algorithm, "signed_curve25519") == 0);

	const char *_signature = get_signature(_key, userid, devid);
	assert(_signature);

	char signature[strlen(_signature)];
	strcpy(signature, _signature);
	int err = verify_signature(_key, signature, identkey);
	if (err == -1) {
		return -1;
	} else if (err) {
		return 1;
	}

	return 0;
}
one_time_key_t *parse_one_time_key(const char *keyidentif, json_object *_key)
{
	one_time_key_t *otkey = malloc(sizeof(*otkey));
	if (!otkey)
		return NULL;
	memset(otkey, 0, sizeof(*otkey));

	const char *algorithm;
	const char *id;
	split_key_identifier(keyidentif, &algorithm, &id);

	if (strrpl(&otkey->algorithm, algorithm))
		goto err_free_otkey;

	if (strrpl(&otkey->id, id))
		goto err_free_otkey;

	if (strcmp(algorithm, "ed25519") == 0 && strcmp(algorithm, "curve25519") == 0) {
		assert(json_object_is_type(_key, json_type_string));
		const char *key = json_object_get_string(_key);
		if (strrpl(&otkey->key, key))
			goto err_free_otkey;
	} else if (strcmp(algorithm, "signed_curve25519") == 0) {

		if (json_rpl_string_(_key, "key", &otkey->key))
			goto err_free_otkey;
	} else {
		assert(0);
	}

	return otkey;

err_free_otkey:
	free_one_time_key(otkey);
	return NULL;
}
int one_time_key_format(OlmAccount *account, const char *userid, const char *devid,
		one_time_key_t *key, char **_keyidentif, json_object **_obj)
{
	char *keyidentif = malloc(get_key_identifier_length(key->algorithm, key->id) + 1);
	if (!keyidentif) {
		return 1;
	}
	get_key_identifier(key->algorithm, key->id, keyidentif);

	json_object *obj;
	if (strcmp(key->algorithm, "ed25519") == 0 && strcmp(key->algorithm, "curve25519") == 0) {
		obj = json_object_new_string(key->key);
		if (!obj)
			goto err_free_formatted_data;
	} else if (strcmp(key->algorithm, "signed_curve25519") == 0) {
		obj = json_object_new_object();
		if (!obj)
			goto err_free_formatted_data;

		if (json_add_string_(obj, "key", key->key))
			goto err_free_formatted_data;

		if (sign_json(account, obj, userid, devid))
			goto err_free_formatted_data;
	} else {
		assert(0);
	}

	*_keyidentif = keyidentif;
	*_obj = obj;
	return 0;

err_free_formatted_data:
	free(keyidentif);
	json_object_put(obj);
	return 1;
}

void free_olm_account(OlmAccount *acc)
{
	if (!acc)
		return;

	olm_clear_account(acc);
	free(acc);
}
OlmAccount *create_olm_account(void)
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

megolm_session_t *create_megolm_session(unsigned long rotperiod, unsigned long rotmsgnum)
{
	megolm_session_t *session = malloc(sizeof(session));
	if (!session)
		return NULL;
	memset(session, 0, sizeof(*session));

	OlmOutboundGroupSession *groupsession = malloc(olm_outbound_group_session_size());
	if (!groupsession) {
		free(session);
		return NULL;
	}
	groupsession = olm_outbound_group_session(groupsession);

	size_t rdlen = olm_init_outbound_group_session_random_length(groupsession);
	void *random = malloc(rdlen);
	if (!random)
		goto err_free_session;

	if (getrandom_(random, rdlen)) {
		free(random);
		goto err_free_session;
	}

	if (olm_init_outbound_group_session(groupsession, random, rdlen) == olm_error()) {
		free(random);
		goto err_free_session;
	}
	free(random);

	session->groupsession = groupsession;
	session->rotperiod = rotperiod;
	session->rotmsgnum = rotmsgnum;

	time_t t = time(NULL);
	if (t == -1)
		goto err_free_session;
	session->tpcreation = t;

	return session;

err_free_session:
	olm_clear_outbound_group_session(groupsession);
	free(session);
	return NULL;
}
void free_megolm_session(megolm_session_t *session)
{
	olm_clear_outbound_group_session(session->groupsession);
	free(session);
}
int update_megolm_session(megolm_session_t **session)
{
	megolm_session_t *s = *session;

	time_t t = time(NULL);
	if (t == -1)
		return 1;

	uint32_t msgnum = olm_outbound_group_session_message_index(s->groupsession);
	if (msgnum < s->rotmsgnum && t - s->tpcreation < s->rotperiod)
		return 0;

	megolm_session_t *_s = create_megolm_session(s->rotperiod, s->rotmsgnum);
	if (!_s)
		return 1;
	free_megolm_session(s);
	s = _s;
	return 0;
}

OlmInboundGroupSession *create_inbound_megolm_session(OlmOutboundGroupSession *outsession)
{
	OlmInboundGroupSession *insession = malloc(olm_inbound_group_session_size());
	if (!insession)
		return NULL;
	insession = olm_inbound_group_session(insession);

	size_t sessionkeylen = olm_outbound_group_session_key_length(outsession);
	uint8_t sessionkey[sessionkeylen];
	if (olm_outbound_group_session_key(outsession, sessionkey, sessionkeylen) == olm_error()) {
		olm_clear_inbound_group_session(insession);
		return NULL;
	}

	if (olm_init_inbound_group_session(insession, sessionkey, sessionkeylen)  == olm_error()) {
		olm_clear_inbound_group_session(insession);
		return NULL;
	}

	return insession;
}

//static json_object *encrypt_megolm(megolm_session_t *session, const char *devid,
//		const char *identkey, const char *roomid, const char *evtype, json_object *evcontent)
//{
//	json_object *payload = json_object_new_object();
//	if (!payload)
//		return NULL;
//
//	if (json_add_string_(payload, "type", evtype)) {
//		json_object_put(payload);
//		return NULL;
//	}
//
//	json_object *content = NULL;
//	if (json_object_deep_copy(evcontent, &content, NULL)) {
//		json_object_put(payload);
//		return NULL;
//	}
//
//	if (json_object_object_add(payload, "content", content)) {
//		json_object_put(content);
//		json_object_put(payload);
//		return NULL;
//		
//	}
//
//	if (json_add_string_(payload, "room_id", roomid)) {
//		json_object_put(payload);
//		return NULL;
//	}
//
//	mtx_ev_encrypted_t crypt;
//	memset(&crypt, 0, sizeof(crypt));
//
//	if (strrpl(&crypt.algorithm, mtx_crypt_algorithm_strs[MTX_CRYPT_ALGORITHM_MEGOLM])) {
//		json_object_put(payload);
//		return NULL;
//	}
//
//	/* encrypt payload */
//	const char *s = json_object_to_json_string_ext(payload, JSON_C_TO_STRING_PLAIN);
//
//	size_t plaintextlen = strlen(s);
//	uint8_t plaintext[plaintextlen];
//	string2byte(s, plaintext, plaintextlen);
//
//	size_t msglen = olm_group_encrypt_message_length(session->groupsession, plaintextlen);
//	uint8_t msg[msglen];
//	if (olm_group_encrypt(session->groupsession, plaintext,
//				plaintextlen, msg, msglen) == olm_error()) {
//		json_object_put(payload);
//		return NULL;
//	}
//
//	char msgstr[msglen + 1];
//	byte2string(msg, msgstr, msglen);
//	msgstr[msglen] = '\0';
//
//	if (strrpl(&crypt.megolm.ciphertext, msgstr)) {
//		json_object_put(payload);
//		return NULL;
//	}
//	json_object_put(payload);
//
//	/* get session id */
//	size_t idlen = olm_outbound_group_session_id_length(session->groupsession);
//	uint8_t id[idlen];
//	if (olm_outbound_group_session_id(session->groupsession, id, idlen) == olm_error())
//		return NULL;
//
//	char idstr[idlen + 1];
//	for (size_t i = 0; i < idlen; ++i) {
//		idstr[i] = (char)id[i];
//	}
//	idstr[idlen] = '\0';
//
//	if (strrpl(&crypt.megolm.sessionid, idstr))
//		return NULL;
//
//	if (strrpl(&crypt.senderkey, identkey))
//		return NULL;
//
//	if (strrpl(&crypt.megolm.deviceid, devid))
//		return NULL;
//
//	json_object *ev = format_ev_encrypted(&crypt);
//	return ev;
//}
//static json_object *decrypt_megolm(megolm_session_t *session, json_object *encrypted)
//{
//	olm_group_decrypt(session->groupsession, )
//}
