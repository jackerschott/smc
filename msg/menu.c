#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <ncurses.h>

#include "api.h"
#include "inputline.h"
#include "menu.h"
#include "room.h"
#include "hcurs.h"

typedef struct {
	WINDOW *win;
	int y;
	int x;
	int height;
	int width;

	size_t nrooms;
	ssize_t selid;
} menu_win_t;

typedef enum {
	ROOMTYPE_JOINED,
	ROOMTYPE_INVITED,
	ROOMTYPE_LEFT,
	ROOMTYPE_NUM,
} room_type_t;

room_type_t seltype;
listentry_t rooms[ROOMTYPE_NUM];

menu_win_t room_wins[ROOMTYPE_NUM];
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
	if (room_wins[seltype].nrooms == 0)
		return;

	listentry_t head = rooms[seltype];
	size_t i = room_wins[seltype].selid;

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
	if (room_wins[seltype].nrooms == 0)
		return 0;

	listentry_t head = rooms[seltype];
	size_t i = room_wins[seltype].selid;

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
	size_t n = room_wins[seltype].nrooms;
	if (n == 0)
		return;

	size_t i = CLAMP(room_wins[seltype].selid + delta, 0, n-1);

	WINDOW *selwin = room_wins[seltype].win;
	wchgat(selwin, -1, 0, 0, NULL);
	wmove(selwin, i + 1, 0);
	wchgat(selwin, -1, A_REVERSE, 0, NULL);

	wrefresh(selwin);

	room_wins[seltype].selid = i;
}
static void change_selected_type(room_type_t newtype)
{
	if (room_wins[newtype].nrooms == 0)
		return;

	WINDOW *oldwin = room_wins[seltype].win;
	WINDOW *newwin = room_wins[newtype].win;

	wchgat(oldwin, -1, 0, 0, NULL);
	wrefresh(oldwin);

	size_t i = room_wins[newtype].selid;
	wmove(newwin, i + 1, 0);
	wchgat(newwin, -1, A_REVERSE, 0, NULL);
	wrefresh(newwin);

	seltype = newtype;
}

static int window_line_print(const menu_win_t *w, attr_t attr, const char *fmt, ...)
{
	va_list args;

	int y, x;
	getyx(w->win, y, x);
	size_t maxlen = w->width - x;

	char *s = malloc(maxlen + 1);
	if (!s)
		return -1;

	va_start(args, fmt);
	size_t n = vsnprintf(s, maxlen + 1, fmt, args);
	va_end(args);
	assert(strlen(s) <= maxlen);

	size_t slen = strlen(s);
	chtype *chstr = malloc((slen + 1) * sizeof(*chstr));
	if (!chstr) {
		free(s);
		return -1;
	}

	for (size_t i = 0; i < slen; ++i) {
		chstr[i] = s[i] | attr;
	}
	free(s);
	chstr[slen] = 0;

	assert(!waddchnstr(w->win, chstr, maxlen));
	free(chstr);

	x += slen;
	if (x > w->width - 1) {
		wmove(w->win, y, w->width - 1);
		wrefresh(w->win);
		return 1;
	} else {
		wmove(w->win, y, x);
		wrefresh(w->win);
		return 0;
	}
}

int room_menu_init(void)
{
	list_init(&rooms[ROOMTYPE_JOINED]);
	list_init(&rooms[ROOMTYPE_INVITED]);
	list_init(&rooms[ROOMTYPE_LEFT]);

	int err;
	if ((err = update()))
		goto err_free_rooms;

	seltype = ROOMTYPE_JOINED;

	int winoff = 0;
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		size_t nrooms = list_length(&rooms[t]);

		int y = winoff;
		int x = 0;
		int height = nrooms + 2;
		int width = COLS;

		WINDOW *win;
		if (!(win = newwin(height, width, winoff, x))) {
			err = -1;
			goto err_free_wins;
		}

		room_wins[t].win = win;
		room_wins[t].y = y;
		room_wins[t].x = x;
		room_wins[t].height = height;
		room_wins[t].width = width;
		room_wins[t].nrooms = nrooms;
		room_wins[t].selid = 0;
		
		winoff += height;
	}

	if ((err = input_line_init()))
		goto err_free_wins;

	return 0;

err_free_wins:
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		delwin(room_wins[t].win);
	}

err_free_rooms:
	list_free(&rooms[ROOMTYPE_LEFT], room_t, entry, free_room);
	list_free(&rooms[ROOMTYPE_INVITED], room_t, entry, free_room);
	list_free(&rooms[ROOMTYPE_JOINED], room_t, entry, free_room);
	return err;
}
void room_menu_cleanup(void)
{
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		delwin(room_wins[t].win);
	}

	list_free(&rooms[ROOMTYPE_LEFT], room_t, entry, free_room);
	list_free(&rooms[ROOMTYPE_INVITED], room_t, entry, free_room);
	list_free(&rooms[ROOMTYPE_JOINED], room_t, entry, free_room);
}

int room_menu_draw_menu_win(menu_win_t *mwin, char *name, listentry_t *rooms)
{
	if (mwin->width <= 0 || mwin->height <= 0)
		return 1;

	WINDOW *w = mwin->win;

	wresize(w, mwin->height, mwin->width);
	assert(mvwin(w, mwin->y, mwin->x) != ERR);

	wmove(w, 0, 0);
	if (window_line_print(mwin, A_BOLD, "%s", name))
		return -1;

	int y = gety(w);
	wmove(w, ++y, 0);

	for (listentry_t *e = rooms->next; e != rooms; e = e->next) {
		if (y >= mwin->height)
			break;

		room_t *r = list_entry(e, room_t, entry);
		
		if (window_line_print(mwin, A_NORMAL, "%s   ", r->name))
			return -1;
		if (window_line_print(mwin, A_NORMAL, "%s   %s", r->topic, r->id))
			return -1;
		wmove(w, ++y, 0);
	}

	wrefresh(w);
	return 0;
}
int room_menu_draw(void)
{
	clear();
	refresh();

	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		WINDOW *w = room_wins[t].win;
		room_wins[t].height = MIN((int)room_wins[t].nrooms + 2, LINES - room_wins[t].y);
		room_wins[t].width = COLS - room_wins[t].x;

		room_menu_draw_menu_win(&room_wins[t], room_type_names[t], &rooms[t]);
	}

	menu_win_t selwin = room_wins[seltype];
	WINDOW *w = selwin.win;
	int i = selwin.selid;
	if (selwin.nrooms > 0 && i < selwin.height - 1) {
		wmove(w, i + 1, 0);
		wchgat(w, -1, A_REVERSE, 0, NULL);
		wrefresh(w);
	}

	return 0;
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
	case KEY_RESIZE:
		if ((err = room_menu_draw()))
			assert(0);
		break;
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
