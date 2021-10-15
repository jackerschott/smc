#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <json-c/json.h>

#include "mtx/mtx.h"
#include "tests/utils.h"

#define DEVID_ALICE_FILE_PATH (TEST_CONFIG_DIR "device_id_bob")
#define DEVID_BOB_FILE_PATH (TEST_CONFIG_DIR "device_id_alice")

static void print_flows(mtx_listentry_t *flows)
{
	int i = 0;
	mtx_list_foreach(flows, mtx_register_flow_t, entry, flow) {
		printf("%i: ", i);

		mtx_list_foreach(&flow->stages, mtx_register_stage_t, entry, stage) {
			printf("%s -> ", stage->type);
		}
		printf("\n");

		++i;
	}
}
static mtx_register_flow_t *select_flow(mtx_listentry_t *flows)
{
	//printf("flow: ");

	//int iflow;
	//if (scanf("%i", &iflow) == EOF)
	//	return NULL;

	//int i = 0;
	//mtx_list_foreach(flows, mtx_register_flow_t, entry, flow) {
	//	if (i == iflow) {
	//		return flow;
	//		break;
	//	}

	//	++i;
	//}

	//return NULL;

	mtx_register_flow_t *_flow;
	mtx_list_entry_content_at(flows, mtx_register_flow_t, entry, 0, &_flow);

	mtx_register_flow_t *flow = mtx_dup_register_flow(_flow);
	if (!flow)
		return NULL;

	return flow;
}
static int register_user(mtx_session_t *session, char *username)
{
	char *devid = NULL;
	mtx_register_flow_t *flow = NULL;
	mtx_listentry_t flows;
	char *sessionkey = NULL;
	while (1) {
		if (mtx_register_user(session, "10.89.64.2:8008", username, username, &devid,
					"smc on linux", 0, flow, &flows, &sessionkey))
			return 1;
		mtx_free_register_flow(flow);

		if (mtx_list_empty(&flows))
			break;

		//print_flows(&flows);
		flow = select_flow(&flows);
		if (!flow)
			return 1;

		mtx_list_free(&flows, mtx_register_flow_t, entry, mtx_free_register_flow);
	}

	return 0;
}
static int init_server(void)
{
	/* setup */
	if (mtx_init())
		goto err_exit;

	mtx_session_t *session = mtx_new_session();
	if (!session)
		goto err_exit;

	/* register alice and bob */
	if (register_user(session, "alice"))
		goto err_exit;

	if (register_user(session, "bob"))
		goto err_exit;

	/* create room as alice */
	if (change_login(session, "alice", 0))
		goto err_exit;

	const char *devid_alice = mtx_device_id(session);
	if (save_config(DEVID_ALICE_FILE_PATH, devid_alice))
		goto err_exit;

	char *roomid;
	if (mtx_create_room_from_preset(session, MTX_ROOM_PRESET_PRIVATE_CHAT, &roomid))
		goto err_exit;

	if (mtx_invite(session, roomid, "@bob:10.89.64.2:8008"))
		goto err_exit;

	/* join room as bob */
	if (change_login(session, "bob", 1))
		goto err_exit;

	const char *devid_bob = mtx_device_id(session);
	if (save_config(DEVID_BOB_FILE_PATH, devid_bob))
		goto err_exit;

	if (mtx_join(session, roomid))
		goto err_exit;

	/* cleanup */
	if (mtx_logout(session))
		goto err_exit;

	free(roomid);

	mtx_free_session(session);
	mtx_cleanup();
	return 0;

err_exit:
	printf("err: %s\n", mtx_last_error_msg());
	return 1;
}

static int get_room(mtx_session_t *session, mtx_room_t **_room)
{
	mtx_sync_response_t *syncresp;
	if (mtx_sync(session, 1, &syncresp))
		return 1;

	if (mtx_apply_sync(session, syncresp))
		return 1;

	mtx_listentry_t rooms;
	mtx_roomlist_init(&rooms);
	if (mtx_roomlist_update(session, &rooms, MTX_ROOM_CONTEXT_JOIN))
		return 1;

	mtx_room_t *room;
	mtx_list_entry_content_at(&rooms, mtx_room_t, entry, 0, &room);
	mtx_room_t *r = mtx_dup_room(room);
	if (!r) {
		mtx_roomlist_free(&rooms);
		return 1;
	}
	*_room = r;

	mtx_roomlist_free(&rooms);
	return 0;
}
static int send_message(mtx_session_t *session, mtx_room_t *room)
{
	char msgstr[] = "Hello World";

	mtx_message_text_t msgtext;
	memset(&msgtext, 0, sizeof(msgtext));

	mtx_ev_message_t evmsg;
	evmsg.type = MTX_MSG_TEXT;
	evmsg.content = &msgtext;
	evmsg.body = msgstr;

	if (mtx_send_message(session, room, &evmsg))
		return 1;

	return 0;
}
static int test(void)
{
	char *devid_alice;
	if (get_config(DEVID_ALICE_FILE_PATH, &devid_alice))
		goto err_exit;

	char *devid_bob;
	if (get_config(DEVID_BOB_FILE_PATH, &devid_bob))
		goto err_exit;

	/* setup */
	if (mtx_init())
		goto err_exit;

	mtx_session_t *session = mtx_new_session();
	if (!session)
		goto err_exit;

	/* send message to bob */
	if (change_login(session, "alice", 0))
		goto err_exit;

	mtx_room_t *room;
	if (get_room(session, &room))
		goto err_exit;

	if (send_message(session, room))
		goto err_exit;
	mtx_free_room(room);

	/* read message from alice */
	if (change_login(session, "bob", 1))
		goto err_exit;

	if (get_room(session, &room))
		goto err_exit;

	mtx_msg_t *msg;
	mtx_list_entry_content_at(&room->msgs, mtx_msg_t, entry, 0, &msg);
	CHECK(msg->type == MTX_MSG_TEXT);
	if (testerr)
		return 1;

	const char *hostname = mtx_hostname(session);
	char sender[1024];
	strcpy(sender, "@alice:");
	strcat(sender, hostname);
	CHECK(strcmp(msg->sender, sender) == 0);
	if (testerr)
		return 1;

	CHECK(strcmp(msg->body, "Hello World") == 0);
	if (testerr)
		return 1;

	mtx_free_room(room);

	/* cleanup */
	if (mtx_logout(session))
		goto err_exit;

	free(devid_alice);
	free(devid_bob);
	mtx_free_session(session);
	mtx_cleanup();
	return 0;

err_exit:
	printf("err: %s\n", mtx_last_error_msg());
	return 1;
}

int main(int argc, char **argv)
{
	return run(argc, argv, init_server, test);
}
