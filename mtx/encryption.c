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

#include <json-c/json.h>
#include <olm/olm.h>

#include "mtx/encryption.h"
#include "lib/hjson.h"
#include "lib/util.h"

const char *algorithm_signing = "ed25519";
const char *crypto_algorithms_msg[] = {
	"m.olm.v1.curve25519-aes-sha2",
	"m.megolm.v1.aes-sha2",
};

OlmAccount *account;
//size_t identity_key_len;
//char *identity_key;
size_t one_time_key_len;
char *one_time_keys;

static int getrandom_(void *buf, size_t len)
{
	void *b = buf;
	size_t remlen = len;
	size_t l;
	while (remlen > 0) {
		if ((l = getrandom(b, remlen, GRND_RANDOM)) == -1)
			return 1;

		b += l;
		remlen -= l;
	}
	assert(remlen == 0);
	return 0;
}

static const char base64url_chars[] = {
'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
'8', '9', '-', '_',
};
static void base64url(const uint8_t *src, size_t n, uint8_t *dest)
{
	for (size_t i = 0; i < n / 3; ++i) {
		int v[4];
		v[0] = src[i * 3 + 0] >> 2;
		v[1] = (src[i * 3 + 0] & 0x3) << 4 | src[i * 3 + 1] >> 4;
		v[2] = (src[i * 3 + 1] & 0xf) << 2 | src[i * 3 + 2] >> 6;
		v[3] = src[i * 3 + 2] & 0x3f;
		dest[i * 4 + 0] = base64url_chars[v[0]];
		dest[i * 4 + 1] = base64url_chars[v[1]];
		dest[i * 4 + 2] = base64url_chars[v[2]];
		dest[i * 4 + 3] = base64url_chars[v[3]];
	}

	/*
	size_t i = n / 3;
	size_t k = n % 3;
	if (k == 1) {
		dest[i * 4 + 0] = base64url_chars[src[i * 3 + 0] >> 2];
		dest[i * 4 + 1] = base64url_chars[(src[i * 3 + 0] & 0x3) << 4];
		dest[i * 4 + 2] = '=';
		dest[i * 4 + 3] = '=';
	} else if (k == 2) {
		dest[i * 4 + 0] = base64url_chars[src[i * 3 + 0] >> 2];
		dest[i * 4 + 1] = base64url_chars[(src[i * 3 + 0] & 0x3) << 4 | src[i * 3 + 1] >> 4];
		dest[i * 4 + 2] = base64url_chars[(src[i * 3 + 1] & 0xf) << 2];
		dest[i * 4 + 3] = '=';
	}
	*/
}
int generate_transaction_id(char *id)
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

static char *get_canonical_json_string(json_object *obj)
{
	return strdup(json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));
}

int create_device_keys(json_object **_keys)
{
	OlmAccount *acc = malloc(olm_account_size());
	if (!acc)
		return 1;
	acc = olm_account(acc);

	size_t rdlen = olm_create_account_random_length(acc);
	void *random = malloc(rdlen);
	if (!random) {
		free(acc);
		return 1;
	}

	if (getrandom_(random, rdlen)) {
		free(random);
		free(acc);
		return 1;
	}

	if (olm_create_account(acc, random, rdlen) == olm_error()) {
		free(random);
		goto err_free_account;
	}
	free(random);

	size_t identkeylen = olm_account_identity_keys_length(acc);
	char *identkey = malloc(identkeylen + 1);
	if (!identkey)
		goto err_free_account;

	if (olm_account_identity_keys(acc, identkey, identkeylen) == olm_error()) {
		free(identkey);
		goto err_free_account;
	}
	identkey[identkeylen] = 0;

	json_object *keys = json_tokener_parse(identkey);
	if (!keys) {
		free(identkey);
		goto err_free_account;
	}
	*_keys = keys;

	account = acc;
	return 0;

err_free_account:
	free(acc);
	return 1;
}

int create_one_time_keys(json_object **_keys)
{
	size_t rdlen = olm_create_account_random_length(account);
	void *random = malloc(rdlen);
	if (!random)
		return 1;

	if (getrandom_(random, rdlen)) {
		free(random);
		return 1;
	}

	size_t nkeys = olm_account_max_number_of_one_time_keys(account) / 2;
	if (olm_account_generate_one_time_keys(account, nkeys, random, rdlen) == olm_error()) {
		free(random);
		return 1;
	}
	free(random);

	size_t otkeylen = olm_account_one_time_keys_length(account);
	char *otkeys = malloc(otkeylen + 1);
	if (!otkeys)
		return 1;
	if (olm_account_one_time_keys(account, otkeys, otkeylen)) {
		free(otkeys);
		return 1;
	}
	otkeys[otkeylen] = 0;

	json_object *keys = json_tokener_parse(otkeys);
	if (!keys) {
		free(otkeys);
		return 1;
	}
	*_keys = keys;
	return 0;
}

int sign_json(json_object *obj, const char *userid, const char *keyident)
{
	json_object *usigned = json_object_object_get(obj, "unsigned");
	json_object *signatures = json_object_object_get(obj, "signatures");
	if (!signatures) {
		signatures = json_object_new_object();
		if (!signatures)
			return 1;
	}

	json_object_object_del(obj, "unsigned");
	json_object_object_del(obj, "signatures");

	char *canonical = get_canonical_json_string(obj);
	if (!canonical)
		goto err_free_excluded;

	size_t signlen = olm_account_signature_length(account);
	void *signature = malloc(signlen);
	if (!signature) {
		free(canonical);
		goto err_free_excluded;
	}

	if (olm_account_sign(account, canonical, strlen(canonical),
				signature, signlen) == olm_error()) {
		free(signature);
		free(canonical);
		goto err_free_excluded;
	}
	free(canonical);

	if (usigned && json_object_object_add(obj, "unsigned", usigned)) {
		free(signature);
		goto err_free_excluded;
	}
	if (json_object_object_add(obj, "signatures", signatures)) {
		free(signature);
		json_object_put(signatures);
		return 1;
	}

	json_object *signobj = json_object_new_object();
	if (!signobj) {
		free(signature);
		return 1;
	}
	if (json_object_object_add(obj, userid, signobj)) {
		free(signature);
		json_object_put(signobj);
		return 1;
	}

	char *signkey = malloc(strlen(signing_algorithm) + STRLEN(":") + strlen(keyident));
	if (!signkey) {
		free(signature);
		return 1;
	}
	if (json_object_add_string_(obj, signkey, signature)) {
		free(signkey);
		free(signature);
		return 1;
	}
	free(signkey);
	free(signature);
	return 0;

err_free_excluded:
	json_object_put(signatures);
	json_object_put(usigned);
	return 1;
}


int start_megolm_session(void)
{
	OlmOutboundGroupSession *session = malloc(olm_outbound_group_session_size());
	if (!session)
		return 1;
	session = olm_outbound_group_session(session);

	size_t rdlen = olm_init_outbound_group_session_random_length(session);
	void *random = malloc(rdlen);
	if (!random)
		goto err_free_session;

	if (getrandom_(random, rdlen)) {
		free(random);
		goto err_free_session;
	}

	if (olm_init_outbound_group_session(session, random, rdlen) == olm_error()) {
		free(random);
		goto err_free_session;
	}

err_free_session:
		free(session);
		return 1;
}
