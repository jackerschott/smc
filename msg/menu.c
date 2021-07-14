#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "api/api.h"
#include "lib/htermbox.h"
#include "msg/inputline.h"
#include "msg/menu.h"
#include "msg/room.h"
#include "msg/sync.h"

typedef struct {
	tb_win_t win;

	size_t nrooms;
	ssize_t selid;
} menu_win_t;

room_type_t seltype;

menu_win_t menu_wins[ROOMTYPE_NUM];
static char * room_type_names[] = {
	"Joined Rooms",
	"Invited Rooms",
	"Left Rooms",
};

static void open_selected_room(void)
{
	if (menu_wins[seltype].nrooms == 0)
		return;

	listentry_t head = smc_rooms[seltype];
	size_t i = menu_wins[seltype].selid;

	listentry_t *e;
	list_entry_at(&head, i, &e);
	assert(e != &head);

	room_t *room = list_entry(e, room_t, entry);
	room_set_room(room);

	room_menu_clear();
	room_draw();
}
static int join_selected_room(void)
{
	if (menu_wins[seltype].nrooms == 0)
		return 0;

	listentry_t head = smc_rooms[seltype];
	size_t i = menu_wins[seltype].selid;

	listentry_t *e;
	list_entry_at(&head, i, &e);
	assert(e != &head);

	room_t *room = list_entry(e, room_t, entry);
	int err = api_join(room->id);
	if (err == 1) {
		input_line_print("api err: %s\n", api_last_errmsg);
		return 0;
	} else if (err != 0) {
		return 1;
	}

	room_menu_draw();
	return 0;
}
static void move_selection(int delta)
{
	size_t n = menu_wins[seltype].nrooms;
	if (n == 0)
		return;

	size_t i = CLAMP(menu_wins[seltype].selid + delta, 0, n-1);

	tb_win_t selwin = menu_wins[seltype].win;
	tb_chattr(&selwin, tb_width(), 1, TB_DEFAULT, TB_DEFAULT);
	tb_wmove(&selwin, 0, i + 1);
	tb_chattr(&selwin, tb_width(), 1, TB_DEFAULT, TB_REVERSE);
	tb_present();

	menu_wins[seltype].selid = i;
}
static void change_selected_type(room_type_t newtype)
{
	if (menu_wins[newtype].nrooms == 0)
		return;

	tb_win_t oldwin = menu_wins[seltype].win;
	tb_win_t newwin = menu_wins[newtype].win;
	size_t i = menu_wins[newtype].selid;

	tb_chattr(&oldwin, tb_width(), 1, TB_DEFAULT, TB_DEFAULT);
	tb_wmove(&newwin, 0, i + 1);
	tb_chattr(&newwin, tb_width(), 1, TB_DEFAULT, TB_REVERSE);
	tb_present();

	seltype = newtype;
}

static void calc_windows()
{
	int winoff = 0;
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		size_t nrooms = list_length(&smc_rooms[t]);

		tb_win_t win;
		win.x = 0;
		win.y = winoff;
		win.height = MIN(nrooms + 2, tb_height() - win.y);
		win.width = tb_width();
		win.wrap = 0;

		menu_wins[t].win = win;
		menu_wins[t].nrooms = nrooms;

		winoff += win.height;
	}
}

int draw_menu_window(tb_win_t *win, char *name, listentry_t *rooms)
{
	if (win->width == 0 || win->height == 0)
		return 1;

	tb_wmove(win, 0, 0);

	int err = 0;
	if ((err = tb_printf(win, TB_DEFAULT | TB_BOLD, TB_DEFAULT, "%s\n", name)))
		return err;

	for (listentry_t *e = rooms->next; e != rooms; e = e->next) {
		room_t *r = list_entry(e, room_t, entry);
		
		if ((err = tb_printf(win, TB_DEFAULT, TB_DEFAULT, "%s   ", r->name)))
			return err;
		if ((err = tb_printf(win, TB_DEFAULT, TB_DEFAULT, "%s   %s\n", r->topic, r->id)))
			return err;
	}

	return 0;
}

int room_menu_init(void)
{
	list_init(&smc_rooms[ROOMTYPE_JOINED]);
	list_init(&smc_rooms[ROOMTYPE_INVITED]);
	list_init(&smc_rooms[ROOMTYPE_LEFT]);

	seltype = ROOMTYPE_JOINED;
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		menu_wins[t].selid = 0;
	}

	input_line_init();
	return 0;

//err_free_rooms:
//	list_free(&smc_rooms[ROOMTYPE_LEFT], room_t, entry, free_room);
//	list_free(&smc_rooms[ROOMTYPE_INVITED], room_t, entry, free_room);
//	list_free(&smc_rooms[ROOMTYPE_JOINED], room_t, entry, free_room);
//	return err;
}
void room_menu_cleanup(void)
{
	list_free(&smc_rooms[ROOMTYPE_LEFT], room_t, entry, free_room);
	list_free(&smc_rooms[ROOMTYPE_INVITED], room_t, entry, free_room);
	list_free(&smc_rooms[ROOMTYPE_JOINED], room_t, entry, free_room);

	//do {
	//	listentry_t *e = (&smc_rooms[ROOMTYPE_JOINED])->next;
	//	while (e != (&smc_rooms[ROOMTYPE_JOINED])) {
	//		listentry_t *next = e->next;
	//		room_t *entry = list_entry(e, room_t, entry);
	//		free_room(entry);
	//		e = next;
	//	}
	//} while (0);
}

int room_menu_draw(void)
{
	tbh_clear();
	calc_windows();

	int err;
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		if ((err = draw_menu_window(&menu_wins[t].win,
						room_type_names[t], &smc_rooms[t]))) {
			if (err == 1)
				break;
			return err;
		}
	}

	int i = menu_wins[seltype].selid;
	menu_win_t selwin = menu_wins[seltype];
	if (selwin.nrooms > 0 && i < selwin.win.height - 1) {
		tb_wmove(&selwin.win, 0, i + 1);
		tb_chattr(&selwin.win, tb_width(), 1, TB_DEFAULT, TB_REVERSE);
	}

	if (input_line_is_active())
		input_line_draw();

	tb_present();
	return 0;
}
void room_menu_clear(void)
{
	tbh_clear();
	tb_present();
}

int room_menu_handle_event(struct tb_event *ev, uimode_t *newmode)
{
	*newmode = MODE_ROOM_MENU;

	int err = 0;
	if (ev->type == TB_EVENT_RESIZE) {
		if ((err = room_menu_draw()))
			assert(0);
	} else if (ev->type == TB_EVENT_MOUSE) {
		return 0;
	}

	switch (ev->ch) {
	case 0:
		break;
	case 'j':
		move_selection(1);
		return err;
	case 'k':;
		move_selection(-1);
		return err;
	case 'a':
		if (seltype != ROOMTYPE_INVITED)
			return err;

		return err;
	case 'J':
		change_selected_type(ROOMTYPE_JOINED);
		return err;
	case 'I':
		change_selected_type(ROOMTYPE_INVITED);
		return err;
	case 'L':
		change_selected_type(ROOMTYPE_LEFT);
		return err;
	default:
		return err;
	}

	switch (ev->key) {
	case TB_KEY_ENTER:
		if (seltype == ROOMTYPE_JOINED) {
			open_selected_room();
			*newmode = MODE_ROOM;
		} else if (seltype == ROOMTYPE_INVITED) {
			err = join_selected_room();
			*newmode = MODE_ROOM;
		}
		return err;
	default:
		return err;
	}
}
int room_menu_handle_sync(void)
{
	return room_menu_draw();
}
