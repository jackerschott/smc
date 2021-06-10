#include <assert.h>
#include <string.h>

#include <ncurses.h>

#include "api.h"
#include "inputline.h"
#include "menu.h"
#include "room.h"

typedef enum {
	ROOMTYPE_JOINED,
	ROOMTYPE_INVITED,
	ROOMTYPE_LEFT,
	ROOMTYPE_NUM,
} room_type_t;

room_type_t seltype;
listentry_t rooms[ROOMTYPE_NUM];

size_t room_nums[ROOMTYPE_NUM];
ssize_t room_selidxs[ROOMTYPE_NUM];
WINDOW *room_wins[ROOMTYPE_NUM];
static char * room_type_names[] = {
	"Joined Rooms",
	"Invited Rooms",
	"Left Rooms",
};

static int update(void)
{
	if (api_sync(&rooms[ROOMTYPE_JOINED], &rooms[ROOMTYPE_INVITED], &rooms[ROOMTYPE_LEFT]))
		return 1;
	return 0;
}

static void open_selected_room(void)
{
	if (room_nums[seltype] == 0)
		return;

	listentry_t head = rooms[seltype];
	size_t i = room_selidxs[seltype];

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
	if (room_nums[seltype] == 0)
		return 0;

	listentry_t head = rooms[seltype];
	size_t i = room_selidxs[seltype];

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

	update();
	room_menu_draw();
	return 0;
}
static void move_selection(int delta)
{
	size_t n = room_nums[seltype];
	if (n == 0)
		return;

	size_t i = CLAMP(room_selidxs[seltype] + delta, 0, n-1);

	WINDOW *selwin = room_wins[seltype];
	wchgat(selwin, -1, 0, 0, NULL);
	wmove(selwin, i + 1, 0);
	wchgat(selwin, -1, A_REVERSE, 0, NULL);

	wrefresh(selwin);

	room_selidxs[seltype] = i;
}
static void change_selected_type(room_type_t newtype)
{
	if (room_nums[newtype] == 0)
		return;

	WINDOW *oldwin = room_wins[seltype];
	WINDOW *newwin = room_wins[newtype];

	wchgat(oldwin, -1, 0, 0, NULL);
	wrefresh(oldwin);

	size_t i = room_selidxs[newtype];
	wmove(newwin, i + 1, 0);
	wchgat(newwin, -1, A_REVERSE, 0, NULL);
	wrefresh(newwin);

	seltype = newtype;
}

int room_menu_init(void)
{
	list_init(&rooms[ROOMTYPE_JOINED]);
	list_init(&rooms[ROOMTYPE_INVITED]);
	list_init(&rooms[ROOMTYPE_LEFT]);
	update();

	seltype = ROOMTYPE_JOINED;
	memset(room_selidxs, 0, sizeof(room_nums));

	int err;
	int winoff = 0;
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		room_nums[t] = list_length(&rooms[t]);

		int height = room_nums[t] + 2;

		WINDOW *win;
		if (!(win = newwin(height, COLS, winoff, 0))) {
			err = -1;
			goto err_free;
		}
		room_wins[t] = win;
		
		winoff += height;
	}

	if ((err = input_line_init()))
		goto err_free;

	return 0;

err_free:
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		delwin(room_wins[t]);
	}

	list_free(&rooms[ROOMTYPE_LEFT], room_t, entry, free_room);
	list_free(&rooms[ROOMTYPE_INVITED], room_t, entry, free_room);
	list_free(&rooms[ROOMTYPE_JOINED], room_t, entry, free_room);
	return err;
}
void room_menu_cleanup(void)
{
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		delwin(room_wins[t]);
	}

	list_free(&rooms[ROOMTYPE_LEFT], room_t, entry, free_room);
	list_free(&rooms[ROOMTYPE_INVITED], room_t, entry, free_room);
	list_free(&rooms[ROOMTYPE_JOINED], room_t, entry, free_room);
}

void room_menu_draw(void)
{
	clear();
	refresh();

	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		WINDOW *w = room_wins[t];
		wmove(w, 0, 0);

		wattron(w, A_BOLD);
		wprintw(w, "%s\n", room_type_names[t]);
		wattroff(w, A_BOLD);

		for (listentry_t *e = rooms[t].next; e != &rooms[t]; e = e->next) {
			room_t *r = list_entry(e, room_t, entry);
			
			wprintw(w, "%s   ", r->name);
			wprintw(w, "%s   %s\n", r->topic, r->id);
		}
		wprintw(w, "\n");

		wrefresh(w);
	}

	if (room_nums[seltype] > 0) {
		WINDOW *selwin = room_wins[seltype];
		size_t i = room_selidxs[seltype];
		wmove(selwin, i + 1, 0);
		wchgat(selwin, -1, A_REVERSE, 0, NULL);
		wrefresh(selwin);
	}
}
void room_menu_clear(void)
{
	clear();
	refresh();
}

int room_menu_handle_key(int c, uimode_t *newmode)
{
	int err = 0;
	switch (c) {
	case 'j':
		move_selection(1);
		*newmode = MODE_ROOM_MENU;
		break;
	case 'k':;
		move_selection(-1);
		*newmode = MODE_ROOM_MENU;
		break;
	case 'a':
		if (seltype != ROOMTYPE_INVITED)
			break;

		break;
	case 'J':
		change_selected_type(ROOMTYPE_JOINED);
		break;
	case 'I':
		change_selected_type(ROOMTYPE_INVITED);
		break;
	case 'L':
		change_selected_type(ROOMTYPE_LEFT);
		break;
	case '\n':
		if (seltype == ROOMTYPE_JOINED) {
			open_selected_room();
			*newmode = MODE_ROOM;
		} else if (seltype == ROOMTYPE_INVITED) {
			err = join_selected_room();
			*newmode = MODE_ROOM;
		}
		break;
	default:
		*newmode = MODE_ROOM;
		break;
	}

	return err;
}
