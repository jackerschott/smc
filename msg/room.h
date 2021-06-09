#ifndef ROOM_H
#define ROOM_H

#include "state.h"
#include "ui.h"

int room_init(void);
void room_cleanup(void);

void room_draw(void);
int room_handle_key(int c, uimode_t *newmode);

void room_set_room(room_t *r);

#endif /* ROOM_H */
