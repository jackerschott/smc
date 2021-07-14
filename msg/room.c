#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* sleep */

#include "api/api.h"
#include "msg/command.h"
#include "msg/inputline.h"
#include "msg/readmsg.h"
#include "msg/room.h"
#include "msg/smc.h"

room_t *room;
listentry_t *messages;

#define MSG_PROMPT "> "
tb_win_t msgwin;
tb_win_t titlewin;
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

static void draw_msg(char *msg, tb_win_t *win)
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

static void calc_windows(void)
{
	titlewin.x = 0;
	titlewin.y = 0;
	titlewin.width = tb_width();
	titlewin.height = 2;
	titlewin.wrap = 0;

	msgwin.x = 0;
	msgwin.y = 2;
	msgwin.width = tb_width();
	msgwin.height = tb_height() - 3;
	msgwin.wrap = 0;
}

void room_init(void)
{
	calc_windows();
}
void room_cleanup(void)
{
	/* empty for now */
}

int room_draw(void)
{
	tbh_clear();
	calc_windows();

	tb_wmove(&titlewin, 0, 0);

	int err;
	if ((err = tb_printf(&titlewin, TB_DEFAULT | TB_BOLD, TB_DEFAULT, "%s   ", room->name)))
		return err;
	if ((err = tb_printf(&titlewin, TB_DEFAULT, TB_DEFAULT, "%s   %s\n", room->topic, room->id)))
		return err;
	tb_present();

	tb_hline(&titlewin, tb_width(), TB_TABLE_HLINE);

	tb_wmove(&msgwin, 0, 0);
	for (listentry_t *e = room->messages.next; e != &room->messages; e = e->next) {
		msg_t *m = list_entry(e, msg_t, entry);

		member_t *mem;
		get_member(m->sender, &mem);

		if ((err = tb_printf(&msgwin, TB_DEFAULT | TB_BOLD, TB_DEFAULT, "%s\n", mem->displayname)))
			return err;
		if ((err = tb_printf(&msgwin, TB_DEFAULT, TB_DEFAULT, "%s\n", m->body)))
			return err;
		if ((err = tb_printf(&msgwin, TB_DEFAULT, TB_DEFAULT, "\n")))
			return err;
	}

	if (input_line_is_active())
		input_line_draw();

	tb_present();
	return 0;
}
void room_clear()
{
	tbh_clear();
	tb_present();
}
void room_set_room(room_t *r)
{
	room = r;
}

int room_handle_normal_key_event(struct tb_event *ev)
{
	input_line_clear();

	switch (ev->ch) {
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
int room_handle_event(struct tb_event *ev, uimode_t *newmode)
{
	int err;
	if (input_line_is_active()) {
		err = input_line_handle_event(ev);
	} else if (ev->type == TB_EVENT_RESIZE) {
		err = room_draw();
	} else if (ev->type == TB_EVENT_KEY) {
		err = room_handle_normal_key_event(ev);
	}
	*newmode = MODE_ROOM;
	return err;
}
int room_handle_sync(void)
{
	return room_draw();
}
