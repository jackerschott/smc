#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* sleep */

#include <ncurses.h>

#include "api.h"
#include "command.h"
#include "inputline.h"
#include "readmsg.h"
#include "room.h"
#include "smc.h"

room_t *room;
listentry_t *messages;

#define MSG_PROMPT "> "
WINDOW *msgwin;
WINDOW *titlewin;
listentry_t *msgwins;

static void get_member(char *userid, member_t **mem)
{
	for (listentry_t *e = room->members.next; e != &room->members; e = e->next) {
		member_t *m = list_entry(e, member_t, entry);
		if (strcmp(userid, m->userid) == 0) {
			*mem = m;
			return;
		}
	}
	assert(0);
}

static void draw_msg(char *msg, WINDOW *win)
{

}
static int send_msg(char *msg)
{
	char *body = strdup(msg);
	if (!body)
		return -1;

	msg_t m;
	m.type = MSG_TEXT;
	m.body = body;

	char *evid;
	if (api_send_msg(room->id, &m, &evid)) {
		free(body);
		return -1;
	}
	free(body);
	free(evid);

	return 0;
}

static int invite_users(size_t nids, char **userids)
{
	int err;
	for (size_t i = 0; i < nids; ++i) {
		err = api_invite(room->id, userids[i]);
		if (err == 1) {
			input_line_print("api err: %s\n", api_last_errmsg);
			return 2;
		} else if (err != 0) {
			return 1;
		}
	}
	return 0;
}
static int handle_command(char *cmd)
{
	command_t c;
	int err = parse_command(cmd, &c);
	if (err == 1) {
		input_line_print("err: %s\n", cmd_last_err);
		return 0;
	} else if (err != 0) {
		return 1;
	}

	if (strcmp(c.name, "invite") == 0) {
		command_invite_t invite = c.invite;
		err = invite_users(invite.nuserids, invite.userids);
		if (err == 2) {
			return 0;
		} else if (err != 0) {
			return 1;
		}
	} else {
		input_line_print("err: unknow command\n");
		return 0;
	}

	return 0;
}

int room_init(void)
{
	if (!(titlewin = newwin(2, COLS, 0, 0))) {
		return -1;
	}
	if (!(msgwin = newwin(LINES - 3, COLS, 2, 0))) {
		delwin(titlewin);
		return -1;
	}
	if (scrollok(msgwin, TRUE) == ERR) {
		delwin(msgwin);
		delwin(titlewin);
		return -1;
	}
	return 0;
}
void room_cleanup(void)
{
	delwin(titlewin);
	delwin(msgwin);
}

void room_draw(void)
{
	wmove(titlewin, 0, 0);
	wattron(titlewin, A_BOLD);
	wprintw(titlewin, "%s   ", room->name);
	wattroff(titlewin, A_BOLD);
	wprintw(titlewin, "%s   %s\n", room->topic, room->id);

	whline(titlewin, ACS_HLINE, COLS);
	wrefresh(titlewin);

	wmove(msgwin, 0, 0);
	for (listentry_t *e = room->messages.next; e != &room->messages; e = e->next) {
		msg_t *m = list_entry(e, msg_t, entry);

		member_t *mem;
		get_member(m->sender, &mem);

		wattron(msgwin, A_BOLD);
		wprintw(msgwin, "%s\n", mem->displayname);
		wattroff(msgwin, A_BOLD);
		wprintw(msgwin, "%s\n", m->body);
		wprintw(msgwin, "\n");
	}
	wrefresh(msgwin);
}
void room_clear()
{
	clear();
	refresh();
}
void room_set_room(room_t *r)
{
	room = r;
}

int room_handle_normal_key(int c)
{
	input_line_clear();

	switch (c) {
	case 'o':
		input_line_start(MSG_PROMPT, send_msg);
		break;
	case ':':
		input_line_start(CMD_PROMPT, handle_command);
		break;
	default:
		break;
	}
	return 0;
}
int room_handle_key(int c, uimode_t *newmode)
{
	int err;
	if (input_line_is_active()) {
		err = input_line_handle_key(c);
	} else {
		err = room_handle_normal_key(c);
	}
	*newmode = MODE_ROOM;
	return err;
}
