#ifndef ROOM_H
#define ROOM_H

#include "api/state.h"
#include "lib/htermbox.h"
#include "msg/ui.h"

void room_init(void);
void room_cleanup(void);

int room_draw(void);
int room_handle_event(struct tb_event *ev, uimode_t *newmode);
int room_handle_sync(void);

void room_set_room(room_t *r);

#endif /* ROOM_H */
