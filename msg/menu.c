#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <ncurses.h>

#include "mtx/mtx.h"
#include "msg/command.h"
#include "msg/inputline.h"
#include "msg/menu.h"
#include "msg/room.h"
#include "msg/sync.h"
#include "msg/smc.h"

typedef struct {
	WINDOW *pad;
	int y;
	int x;
	int height;
	int width;

	size_t nrooms;
	ssize_t selid;
} menu_win_t;

mtx_listentry_t menu_rooms[MTX_ROOM_CONTEXT_NUM];

mtx_room_context_t selected_context;

menu_win_t menu_wins[MTX_ROOM_CONTEXT_NUM];
static char * room_context_names[] = {
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
	menu_win_t *selmwin = &menu_wins[selected_context];
	if (selmwin->nrooms == 0)
		return 0;

	mtx_listentry_t *e;
	mtx_list_entry_at(&menu_rooms[selected_context], selmwin->selid, &e);
	smc_cur_room = mtx_list_entry_content(e, mtx_room_t, entry);

	if (!room_is_initialized()) {
		if (room_init())
			return 1;
	}
	if (room_draw())
		return 1;
	return 0;
}
//static int join_selected_room(void)
//{
//	menu_win_t *selmwin = &menu_wins[selected_context];
//	if (selmwin->nrooms == 0)
//		return 1;
//
//	pthread_mutex_lock(&smc_synclock);
//	mtx_listentry_t *e;
//	mtx_list_entry_at(&menu_rooms[selected_context], selmwin->selid, &e);
//	mtx_room_t *room = mtx_list_entry_content(e, mtx_room_t, entry);
//	pthread_mutex_unlock(&smc_synclock);
//
//	int err = api_join(room->id);
//	if (err == 1) {
//		input_line_print("api err: %s\n", api_last_errmsg);
//		return 0;
//	} else if (err != 0) {
//		return 1;
//	}
//
//	room_menu_draw();
//	return 0;
//}

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
	menu_win_t *selmwin = &menu_wins[selected_context];

	size_t n = selmwin->nrooms;
	if (n == 0)
		return;

	size_t i = CLAMP(menu_wins[selected_context].selid + delta, 0, n - 1);

	wmove(selmwin->pad, selmwin->selid + 1, 0);
	wchgat(selmwin->pad, -1, A_NORMAL, 0, NULL);
	wmove(selmwin->pad, i + 1, 0);
	wchgat(selmwin->pad, -1, A_REVERSE, 0, NULL);
	refresh_menu_window(selmwin);

	selmwin->selid = i;
}
static void change_selected_type(mtx_room_context_t newtype)
{
	menu_win_t *oldmwin = &menu_wins[selected_context];
	menu_win_t *newmwin = &menu_wins[newtype];
	if (oldmwin->nrooms == 0 || newmwin->nrooms == 0)
		return;

	wmove(oldmwin->pad, oldmwin->selid + 1, 0);
	wchgat(oldmwin->pad, -1, A_NORMAL, 0, NULL);
	refresh_menu_window(oldmwin);

	wmove(newmwin->pad, newmwin->selid + 1, 0);
	wchgat(newmwin->pad, -1, A_REVERSE, 0, NULL);
	refresh_menu_window(newmwin);

	selected_context = newtype;
}

static void calc_menu_window(menu_win_t *mwin, mtx_room_context_t context,
		size_t nrooms, mtx_listentry_t *rooms, int selid, int winoff)
{
	int width = strlen(room_context_names[context]);
	mtx_list_foreach(rooms, mtx_room_t, entry, r) {
		int l = strlen(r->name) + STRLEN("  ") + strlen(r->topic);
		if (l > width)
			width = l;
	}
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
	for (mtx_room_context_t ctx = MTX_ROOM_CONTEXT_JOIN; ctx < MTX_ROOM_CONTEXT_NUM; ++ctx) {
		delwin(menu_wins[ctx].pad);
	}
}
static int init_windows()
{
	/* especially ensure wins are zero (for free)*/
	memset(menu_wins, 0, sizeof(menu_wins));

	int winoff = 0;
	for (mtx_room_context_t ctx = MTX_ROOM_CONTEXT_JOIN; ctx < MTX_ROOM_CONTEXT_NUM; ++ctx) {
		int nrooms = mtx_list_length(&menu_rooms[ctx]);

		menu_win_t mwin;
		calc_menu_window(&mwin, ctx, nrooms, &menu_rooms[ctx], 0, winoff);
		WINDOW *pad = newpad(mwin.height + 1, mwin.width);
		if (!pad) {
			free_windows();
			return 1;
		}
		mwin.pad = pad;
		menu_wins[ctx] = mwin;

		winoff += mwin.height + 1;
	}
	return 0;
}
static int draw_menu_window(menu_win_t *mwin, char *name, mtx_listentry_t *rooms, int selected)
{
	WINDOW *pad = mwin->pad;
	wmove(pad, 0, 0);

	wattron(pad, A_BOLD);
	wprintw(pad, "%s\n", name);
	wattroff(pad, A_BOLD);
	refresh_menu_window(mwin);

	mtx_list_foreach(rooms, mtx_room_t, entry, r) {
		wprintw(pad, "%s  %s\n", r->name, r->topic);
		refresh_menu_window(mwin);
	}
	pthread_mutex_unlock(&smc_synclock);

	if (selected && mwin->nrooms > 0) {
		wmove(pad, mwin->selid + 1, 0);
		wchgat(pad, -1, A_REVERSE, 0, NULL);
	}

	refresh_menu_window(mwin);
	return 0;
}

int room_menu_init(void)
{
	for (mtx_room_context_t ctx = MTX_ROOM_CONTEXT_JOIN; ctx < MTX_ROOM_CONTEXT_NUM; ++ctx) {
		mtx_roomlist_init(&menu_rooms[ctx]);
	}

	selected_context = MTX_ROOM_CONTEXT_JOIN;
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

	for (mtx_room_context_t ctx = MTX_ROOM_CONTEXT_JOIN; ctx < MTX_ROOM_CONTEXT_NUM; ++ctx) {
		mtx_roomlist_free(&menu_rooms[ctx]);
	}
}

int room_menu_draw(void)
{
	clear();
	refresh();

	int err;
	int winoff = 0;
	for (mtx_room_context_t ctx = MTX_ROOM_CONTEXT_JOIN; ctx < MTX_ROOM_CONTEXT_NUM; ++ctx) {
		int nrooms = mtx_list_length(&menu_rooms[ctx]);
		calc_menu_window(&menu_wins[ctx], ctx, nrooms, &menu_rooms[ctx],
				menu_wins[ctx].selid, winoff);
		wresize(menu_wins[ctx].pad, menu_wins[ctx].height + 1, menu_wins[ctx].width);
		winoff += menu_wins[ctx].height + 1;

		draw_menu_window(&menu_wins[ctx], room_context_names[ctx],
				&menu_rooms[ctx], ctx == selected_context);
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
		if (selected_context == MTX_ROOM_CONTEXT_JOIN) {
			err = open_selected_room();
			*newmode = MODE_ROOM;
		} else if (selected_context == MTX_ROOM_CONTEXT_INVITE) {
			assert(0); //err = join_selected_room();
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
		if (selected_context != MTX_ROOM_CONTEXT_INVITE)
			return err;

		return err;
	case 'J':
		change_selected_type(MTX_ROOM_CONTEXT_JOIN);
		return err;
	case 'I':
		change_selected_type(MTX_ROOM_CONTEXT_INVITE);
		return err;
	case 'L':
		change_selected_type(MTX_ROOM_CONTEXT_LEAVE);
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
	int clean = 1;
	for (mtx_room_context_t ctx = MTX_ROOM_CONTEXT_JOIN; ctx < MTX_ROOM_CONTEXT_NUM; ++ctx) {
		pthread_mutex_lock(&smc_synclock);
		int dirty = mtx_has_dirty_rooms(smc_session, ctx);
		pthread_mutex_unlock(&smc_synclock);
		if (!dirty)
			continue;

		pthread_mutex_lock(&smc_synclock);
		int err = mtx_roomlist_update(smc_session, &menu_rooms[ctx], ctx);
		pthread_mutex_unlock(&smc_synclock);
		if (err)
			return 1;

		clean = 0;
	}

	if (!clean && room_menu_draw())
		return 1;

	return 0;
}
