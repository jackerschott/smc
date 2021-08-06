#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <ncurses.h>

#include "api/api.h"
#include "lib/hcurs.h"
#include "msg/command.h"
#include "msg/inputline.h"
#include "msg/menu.h"
#include "msg/room.h"
#include "msg/sync.h"

typedef struct {
	WINDOW *pad;
	int y;
	int x;
	int height;
	int width;

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

static int handle_command(char *cmd)
{
	command_t *c;
	int err = parse_command(cmd, &c);
	if (err == 1) {
		input_line_print("err: %s", cmd_last_err);
		return 0;
	} else if (err == 2) {
		return 0;
	} else if (err == -1) {
		return 1;
	}

	input_line_print("err: unknow command\n");

	free_cmd(c);
	return 0;
}

static int open_selected_room(void)
{
	menu_win_t *selmwin = &menu_wins[seltype];
	if (selmwin->nrooms == 0)
		return 0;

	pthread_mutex_lock(&smc_synclock);
	listentry_t *e;
	list_entry_at(&smc_rooms[seltype], selmwin->selid, &e);
	smc_cur_room = list_entry_content(e, room_t, entry);
	pthread_mutex_unlock(&smc_synclock);

	if (!room_is_initialized()) {
		if (room_init())
			return 1;
	}
	if (room_draw())
		return 1;
	return 0;
}
static int join_selected_room(void)
{
	menu_win_t *selmwin = &menu_wins[seltype];
	if (selmwin->nrooms == 0)
		return 1;

	pthread_mutex_lock(&smc_synclock);
	listentry_t *e;
	list_entry_at(&smc_rooms[seltype], selmwin->selid, &e);
	room_t *room = list_entry_content(e, room_t, entry);
	pthread_mutex_unlock(&smc_synclock);

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

static int refresh_menu_window(menu_win_t *mwin)
{
	int h = MIN(mwin->height, LINES - mwin->y);
	int w = MIN(mwin->width, COLS - mwin->x);
	if (h <= 0 || w <= 0)
		return 1;

	prefresh(mwin->pad, 0, 0, mwin->y, mwin->x, mwin->y + h - 1, mwin->x + w - 1);
	return 0;
}

static void move_selection(int delta)
{
	menu_win_t *selmwin = &menu_wins[seltype];

	size_t n = selmwin->nrooms;
	if (n == 0)
		return;

	size_t i = CLAMP(menu_wins[seltype].selid + delta, 0, n - 1);

	wmove(selmwin->pad, selmwin->selid + 1, 0);
	wchgat(selmwin->pad, -1, A_NORMAL, 0, NULL);
	wmove(selmwin->pad, i + 1, 0);
	wchgat(selmwin->pad, -1, A_REVERSE, 0, NULL);
	refresh_menu_window(selmwin);

	selmwin->selid = i;
}
static void change_selected_type(room_type_t newtype)
{
	menu_win_t *oldmwin = &menu_wins[seltype];
	menu_win_t *newmwin = &menu_wins[newtype];
	if (oldmwin->nrooms == 0 || newmwin->nrooms == 0)
		return;

	wmove(oldmwin->pad, oldmwin->selid + 1, 0);
	wchgat(oldmwin->pad, -1, A_NORMAL, 0, NULL);
	refresh_menu_window(oldmwin);

	wmove(newmwin->pad, newmwin->selid + 1, 0);
	wchgat(newmwin->pad, -1, A_REVERSE, 0, NULL);
	refresh_menu_window(newmwin);

	seltype = newtype;
}

static void calc_menu_window(menu_win_t *mwin, room_type_t type,
		size_t nrooms, listentry_t *rooms, int selid, int winoff)
{
	pthread_mutex_lock(&smc_synclock);
	int width = strlen(room_type_names[type]);
	for (listentry_t *e = rooms->next; e != rooms; e = e->next) {
		room_t *r = list_entry_content(e, room_t, entry);

		int l = strlen(r->name) + STRLEN("  ") + strlen(r->topic);
		if (l > width)
			width = l;
	}
	pthread_mutex_unlock(&smc_synclock);
	width += 1;

	mwin->y = winoff;
	mwin->x = 0;
	mwin->height = nrooms + 1;
	mwin->width = MAX(width, COLS);
	mwin->nrooms = nrooms;
	mwin->selid = MIN(selid, nrooms - 1);
}
static void free_windows()
{
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		delwin(menu_wins[t].pad);
	}
}
static int init_windows()
{
	/* especially ensure wins are zero (for free)*/
	memset(menu_wins, 0, sizeof(menu_wins));

	int winoff = 0;
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		int nrooms = list_length(&smc_rooms[t]);

		menu_win_t mwin;
		calc_menu_window(&mwin, t, nrooms, &smc_rooms[t], 0, winoff);
		WINDOW *pad = newpad(mwin.height + 1, mwin.width);
		if (!pad) {
			free_windows();
			return 1;
		}
		mwin.pad = pad;
		menu_wins[t] = mwin;

		winoff += mwin.height + 1;
	}
	return 0;
}
static int draw_menu_window(menu_win_t *mwin, char *name, listentry_t *rooms, int selected)
{
	WINDOW *pad = mwin->pad;
	wmove(pad, 0, 0);

	wattron(pad, A_BOLD);
	wprintw(pad, "%s\n", name);
	wattroff(pad, A_BOLD);
	refresh_menu_window(mwin);

	pthread_mutex_lock(&smc_synclock);
	for (listentry_t *e = rooms->next; e != rooms; e = e->next) {
		room_t *r = list_entry_content(e, room_t, entry);
		wprintw(pad, "%s  %s\n", r->name, r->topic);
		refresh_menu_window(mwin);

		r->dirty = 0;
	}
	pthread_mutex_unlock(&smc_synclock);

	if (selected && mwin->nrooms > 0) {
		wmove(pad, mwin->selid + 1, 0);
		wchgat(pad, -1, A_REVERSE, 0, NULL);
	}

	refresh_menu_window(mwin);
	return 0;
}

static int rooms_are_dirty(void)
{
	pthread_mutex_lock(&smc_synclock);
	int dirty = 0;
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		listentry_t *rooms = &smc_rooms[t];
		for (listentry_t *e = rooms->next; e != rooms; e = e->next) {
			room_t *r = list_entry_content(e, room_t, entry);
			if (r->dirty) {
				dirty = 1;
				break;
			}
		}
	}
	pthread_mutex_unlock(&smc_synclock);
	return dirty;
}

int room_menu_init(void)
{
	seltype = ROOMTYPE_JOINED;
	if (init_windows())
		return 1;

	input_line_init();
	return 0;
}
void room_menu_cleanup(void)
{
	input_line_cleanup();

	if (room_is_initialized())
		room_cleanup();

	free_windows();
}

int room_menu_draw(void)
{
	clear();
	refresh();

	int err;
	int winoff = 0;
	for (room_type_t t = ROOMTYPE_JOINED; t < ROOMTYPE_NUM; ++t) {
		int nrooms = list_length(&smc_rooms[t]);
		calc_menu_window(&menu_wins[t], t, nrooms, &smc_rooms[t],
				menu_wins[t].selid, winoff);
		wresize(menu_wins[t].pad, menu_wins[t].height + 1, menu_wins[t].width);
		winoff += menu_wins[t].height + 1;

		draw_menu_window(&menu_wins[t], room_type_names[t], &smc_rooms[t], t == seltype);
	}

	if (input_line_is_active())
		input_line_draw();

	return 0;
}

int room_menu_handle_event(int ch, uimode_t *newmode)
{
	*newmode = MODE_ROOM_MENU;

	if (ch == KEY_RESIZE) {
		if (room_menu_draw())
			return 1;
	}

	if (input_line_is_active()) {
		if (input_line_handle_event(ch))
			return 1;
		return 0;
	}

	int err = 0;
	switch (ch) {
	case '\n':
		if (seltype == ROOMTYPE_JOINED) {
			err = open_selected_room();
			*newmode = MODE_ROOM;
		} else if (seltype == ROOMTYPE_INVITED) {
			err = join_selected_room();
			*newmode = MODE_ROOM;
		}
		return err;
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
	case ':':
		if (input_line_start(CMD_PROMPT, handle_command))
			return 1;
		return err;
	case 'q':
		pthread_mutex_lock(&smc_synclock);
		smc_terminate = 1;
		pthread_mutex_unlock(&smc_synclock);
		return err;
	default:
		return err;
	}
}
int room_menu_handle_sync(void)
{
	if (!rooms_are_dirty())
		return 0;

	if (room_menu_draw())
		return 1;
	return 0;
}
