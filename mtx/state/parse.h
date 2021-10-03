#ifndef MTX_STATE_PARSE_H
#define MTX_STATE_PARSE_H

#include <json-c/json_types.h>

#include "lib/list.h"
#include "mtx/state/room.h"

int is_roomevent(mtx_eventtype_t type);
int is_statevent(mtx_eventtype_t type);
int is_message_event(mtx_eventtype_t type);

int update_room_histories(json_object *obj, mtx_listentry_t *joined,
		mtx_listentry_t *invited, mtx_listentry_t *left);

int get_presence(const json_object *obj, mtx_listentry_t *events);
int get_account_data(const json_object *obj, mtx_listentry_t *events);

#endif /* MTX_STATE_PARSE_H */
