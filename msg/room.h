#ifndef ROOM_H
#define ROOM_H

#include "msg/ui.h"

int room_init(void);
void room_cleanup(void);
int room_is_initialized(void);

int room_draw(void);

int room_handle_event(int ch, uimode_t *newmode);
int room_handle_sync(void);

#endif /* ROOM_H */
