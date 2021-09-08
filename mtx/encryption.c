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

OlmAccount *account;
//size_t identity_key_len;
//char *identity_key;
size_t one_time_key_len;
char *one_time_keys;

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
