#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* sleep */

#include <ncurses.h>

#include "mtx/mtx.h"
#include "msg/command.h"
#include "msg/inputline.h"
#include "msg/readmsg.h"
#include "msg/room.h"
#include "msg/smc.h"
#include "msg/sync.h"

#define MSG_PROMPT "> "

#define MSGLINES_BUFSIZE 16

typedef struct {

	WINDOW *pad;
	int y;
	int x;
	int height;
	int width;
} pad_t;

typedef struct {
	mtx_listentry_t entry;

	mtx_msg_t *m;
	mtx_member_t *mem;
	char *content;
	int height;
	int width;
} msgbox_t;

pad_t titlewin;
pad_t msgwin;
mtx_listentry_t msgboxes;
int initialized = 0;

//static int send_msg(char *msg)
//{
//	char *body = strdup(msg);
//	if (!body)
//		return -1;
//
//	mtx_msg_t m;
//	m.type = MSG_TEXT;
//	m.body = body;
//
//	char *evid;
//	if (api_send_msg(smc_cur_room->id, &m, &evid)) {
//		free(body);
//		return -1;
//	}
//	free(body);
//	free(evid);
//
//	return 0;
//}

//static int invite_users(size_t nids, char **userids)
//{
//	int err;
//	for (size_t i = 0; i < nids; ++i) {
//		err = api_invite(smc_cur_room->id, userids[i]);
//		if (err == 1) {
//			input_line_print("api err: %s\n", api_last_errmsg);
//			return 2;
//		} else if (err != 0) {
//			return 1;
//		}
//	}
//	return 0;
//}
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

	if (strcmp(c->name, "invite") == 0) {
		command_invite_t invite = c->invite;
		assert(0); //err = invite_users(invite.nuserids, invite.userids);
		if (err == 2) {
			return 0;
		} else if (err != 0) {
			return 1;
		}
	} else {
		input_line_print("err: unknow command\n");
		return 0;
	}

	free_cmd(c);
	return 0;
}

static void free_msgbox(msgbox_t *mb)
{
	free(mb->content);
	free(mb);
}

static void calc_titlewin(void)
{
	int w = strlen(smc_cur_room->name) + STRLEN("  ") + strlen(smc_cur_room->topic) + 1;

	titlewin.y = 0;
	titlewin.x = 0;
	titlewin.height = 2;
	titlewin.width = MAX(w, COLS);
}
static int calc_msgbox(msgbox_t *mb, mtx_member_t *mem, mtx_msg_t *m)
{
	char *content = malloc(strlen(m->body) + 1);
	if (!content)
		return 1;
	strcpy(content, m->body);

	int w = MAX(strlen(mem->displayname), COLS);
	int h = strlen(m->body) / w + 2;

	mb->m = m;
	mb->mem = mem;
	mb->content = content;
	mb->height = h;
	mb->width = w;
	return 0;
}
static int calc_msgwin(void)
{
	mtx_list_free(&msgboxes, msgbox_t, entry, free_msgbox);
	mtx_list_init(&msgboxes);

	int off = 0;
	int maxwidth = 0;
	mtx_listentry_t *msgs = &smc_cur_room->msgs;
	mtx_list_foreach(msgs, mtx_msg_t, entry, m) {
		msgbox_t *mb = malloc(sizeof(*mb));
		if (!mb)
			goto err_free_msgboxes;
		memset(mb, 0, sizeof(*mb));

		mtx_member_t *mem = mtx_find_member(&smc_cur_room->members, m->sender);
		if (calc_msgbox(mb, mem, m)) {
			free(mb);
			goto err_free_msgboxes;
		}
		off += mb->height + 1;
		if (mb->width > maxwidth)
			maxwidth = mb->width;

		mtx_list_add(&msgboxes, &mb->entry);
	}

	msgwin.y = 2;
	msgwin.x = 0;
	msgwin.height = MAX(off - 1, 0);
	msgwin.width = maxwidth + 1;
	return 0;

err_free_msgboxes:
	mtx_list_free(&msgboxes, msgbox_t, entry, free_msgbox);
	return 1;
}

static void free_windows(void)
{
	mtx_list_free(&msgboxes, msgbox_t, entry, free_msgbox);
}
static int init_windows(void)
{
	calc_titlewin();
	WINDOW *titlepad = newpad(titlewin.height + 1, titlewin.width);
	if (!titlepad)
		return 1;
	titlewin.pad = titlepad;

	mtx_list_init(&msgboxes);
	calc_msgwin();

	WINDOW *msgpad = newpad(msgwin.height + 1, msgwin.width);
	if (!msgpad)
		goto err_free_msgboxes;
	msgwin.pad = msgpad;
	return 0;

err_free_msgwin:
	delwin(msgpad);
err_free_msgboxes:
	mtx_list_free(&msgboxes, msgbox_t, entry, free_msgbox);
	delwin(titlepad);
	return 1;
}

static int refresh_pad(pad_t *p)
{
	int h = MIN(p->height, LINES - p->y);
	int w = MIN(p->width, COLS - p->x);
	if (h <= 0 || w <= 0)
		return 1;

	assert(prefresh(p->pad, 0, 0, p->y, p->x, p->y + h - 1, p->x + w - 1) != ERR);
	return 0;
}
static int refresh_msgwin(pad_t *p)
{
	int h = LINES - p->y;
	int w = MIN(p->width, COLS - p->x);
	if (h <= 0 || w <= 0)
		return 1;

	int py = p->height - h;
	assert(prefresh(p->pad, py, 0, p->y, p->x, p->y + h - 1, p->x + w - 1) != ERR);
	return 0;
}
static void draw_title(void)
{
	wclear(titlewin.pad);

	wmove(titlewin.pad, 0, 0);

	wattron(titlewin.pad, A_BOLD);
	wprintw(titlewin.pad, "%s", smc_cur_room->name);
	wattroff(titlewin.pad, A_BOLD);
	refresh_pad(&titlewin);

	wprintw(titlewin.pad, "  %s\n", smc_cur_room->topic);
	refresh_pad(&titlewin);

	whline(titlewin.pad, ACS_HLINE, titlewin.width);
	refresh_pad(&titlewin);
}
static void draw_message_window(void)
{
	wclear(msgwin.pad);

	//int ymax = getmaxy(msgwin.pad);
	//wmove(msgwin.pad, ymax - 5, 0);
	//wprintw(msgwin.pad, "This is a test!");
	//refresh_msgwin(&msgwin);

	wmove(msgwin.pad, 0, 0);
	for (mtx_listentry_t *e = msgboxes.next; e != &msgboxes; e = e->next) {
		msgbox_t *mb = mtx_list_entry_content(e, msgbox_t, entry);

		wattron(msgwin.pad, A_BOLD);
		wprintw(msgwin.pad, "%s\n", mb->mem->displayname);
		wattroff(msgwin.pad, A_BOLD);
		refresh_msgwin(&msgwin);

		waddstr(msgwin.pad, mb->content);
		refresh_msgwin(&msgwin);

		if (e != msgboxes.prev)
			wmove(msgwin.pad, getcury(msgwin.pad) + 2, 0);
	}
	refresh_msgwin(&msgwin);
}

static int handle_normal_key_event(int ch)
{
	switch (ch) {
	case 'o':
		assert(0);
		//if (input_line_start(MSG_PROMPT, send_msg))
		//	return 1;
		break;
	case ':':
		input_line_start(CMD_PROMPT, handle_command);
		break;
	case 'q':
		pthread_mutex_lock(&smc_synclock);
		smc_terminate = 1;
		pthread_mutex_unlock(&smc_synclock);
		break;
	default:
		break;
	}
	return 0;
}

int room_init(void)
{
	if (init_windows())
		return 1;
	input_line_init();
	initialized = 1;
	return 0;
}
void room_cleanup(void)
{
	input_line_cleanup();

	free_windows();
	initialized = 0;
}
int room_is_initialized(void)
{
	return initialized;
}

int room_draw(void)
{
	clear();
	refresh();

	calc_titlewin();
	wresize(titlewin.pad, titlewin.height + 1, titlewin.width);

	if (calc_msgwin())
		return 1;
	wresize(msgwin.pad, msgwin.height + 1, msgwin.width);

	draw_title();
	draw_message_window();

	if (input_line_is_active())
		input_line_draw();

	pthread_mutex_lock(&smc_synclock);
	smc_cur_room->dirty = 0;
	pthread_mutex_unlock(&smc_synclock);
	return 0;
}

int room_handle_event(int ch, uimode_t *newmode)
{
	*newmode = MODE_ROOM;

	if (ch == KEY_RESIZE) {
		if (room_draw())
			return 1;
	}

	if (input_line_is_active()) {
		if (input_line_handle_event(ch))
			return 1;
		return 0;
	}

	if (handle_normal_key_event(ch))
		return 1;
	return 0;
}
int room_handle_sync(void)
{
	pthread_mutex_lock(&smc_synclock);
	int dirty = smc_cur_room->dirty;
	pthread_mutex_unlock(&smc_synclock);
	if (!dirty)
		return 0;

	if (room_draw())
		return 1;
	return 0;
}
