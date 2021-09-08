#ifndef MTX_STATE_H
#define MTX_STATE_H

#include <json-c/json_types.h>

#include "lib/list.h"
#include "mtx/state/room.h"

int is_roomevent(eventtype_t type);
int is_statevent(eventtype_t type);
int is_message_event(eventtype_t type);

int update_room_histories(json_object *obj, listentry_t *joined,
		listentry_t *invited, listentry_t *left);

int get_presence(const json_object *obj, listentry_t *events);
int get_account_data(const json_object *obj, listentry_t *events);

#endif /* MTX_STATE_H */
