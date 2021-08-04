#ifndef MENU_H
#define MENU_H

#include "lib/list.h"
#include "api/state.h"
#include "msg/ui.h"

int room_menu_init(void);
void room_menu_cleanup(void);
int room_menu_draw(void);
void room_menu_clear(void);

int room_menu_handle_event(int ch, uimode_t *newmode);
int room_menu_handle_sync(void);

#endif /* MENU_H */
