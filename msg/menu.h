#ifndef MENU_H
#define MENU_H

#include "list.h"
#include "state.h"
#include "ui.h"

int room_menu_init(void);
void room_menu_cleanup(void);
void room_menu_draw(void);
void room_menu_clear(void);

int room_menu_handle_key(int c, uimode_t *newmode);

#endif /* MENU_H */
