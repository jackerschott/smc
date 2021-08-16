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

#define TRANSACTION_ID_SIZE 44

extern const char *crypto_algorithms_msg[2];

int generate_transaction_id(char *id);

int create_device_keys(json_object **_keys);
int create_one_time_keys(json_object **_keys);

int sign_json(json_object *obj, const char *userid, const char *keyident);

#endif /* ENCRYPTION_H */
