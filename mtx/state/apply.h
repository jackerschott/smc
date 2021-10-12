#ifndef MTX_STATE_APPLY_H
#define MTX_STATE_APPLY_H

#include "lib/list.h"
#include "mtx/state/room.h"

int compute_room_state_from_history(mtx_room_t *room);

int apply_to_device_events(const mtx_listentry_t *events);

#endif /* MTX_STATE_APPLY_H */
