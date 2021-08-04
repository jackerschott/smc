#include <assert.h>
#include <json-c/json.h>
#include <string.h>
#include <stdio.h>

#include "api/state.h"
#include "lib/hjson.h"
#include "msg/smc.h"

typedef enum {
	/* roomevents */
	/* state events */
	M_ROOM_CREATE,
	M_ROOM_MEMBER,
	M_ROOM_JOINRULES,
	M_ROOM_POWERLEVELS,
	M_ROOM_CANONICAL_ALIAS,

	M_ROOM_NAME,
	M_ROOM_TOPIC,

	M_ROOM_HISTORY_VISIBILITY,

	/* message events */
	M_ROOM_REDACTION,

	M_ROOM_MESSAGE,

	EVENT_TYPE_NUM,
} event_type_t;
#define STATEVENT_FIRST M_ROOM_CREATE
#define STATEVENT_LAST M_ROOM_HISTORY_VISIBILITY
static const char *event_type_strs[] = {
	"m.room.create",
	"m.room.member",
	"m.room.join_rules",
	"m.room.power_levels",
	"m.room.canonical_alias",

	"m.room.name",
	"m.room.topic",

	"m.room.history_visibility",

	"m.room.redaction",
	"m.room.message",
};
typedef struct {
	char *id;
	char *sender;
	long ts;
	long age;
	char *transactid;

	char *statekey;
} roomevent_meta_t;

void free_member(member_t *member)
{
	free(member->avatarurl);
	free(member->displayname);
	free(member->userid);
	free(member);
}
void free_powerlevel(powerlevel_t *powerlevel)
{
	free(powerlevel->name);
	free(powerlevel);
}
void free_msg_text(msg_text_t *msg)
{
	free(msg->fmtbody);
	free(msg->format);
	free(msg);
}
void free_msg(msg_t *msg)
{
	free(msg->sender);
	free(msg->body);

	switch (msg->type) {
	case MSG_TEXT:;
		msg_text_t *m = CONTAINER(msg, msg_text_t, msg);
		free_msg_text(m);
		break;
	default:
		assert(0);
	}
}

int get_new_room(const char *roomid, room_t **room)
{
	char *id = strdup(roomid);
	if (!id)
		return -1;

	char *version = strdup("1");
	if (!version) {
		free(id);
	}

	room_t *r = malloc(sizeof(*r));
	if (!r) {
		free(version);
		free(id);
		return -1;
	}

	r->id = id;
	r->name = NULL;
	r->topic = NULL;
	list_init(&r->members);

	r->powerlevels.invite = 50;
	r->powerlevels.kick = 50;
	r->powerlevels.ban = 50;
	r->powerlevels.redact = 50;
	r->powerlevels.statedefault = 50;
	r->powerlevels.eventsdefault = 0;
	list_init(&r->powerlevels.events);
	r->powerlevels.usersdefault = 0;
	list_init(&r->powerlevels.users);
	r->powerlevels.notif.room = 50;

	r->joinrule = JOINRULE_NUM;
	r->histvisib = HISTVISIB_NUM;
	r->creator = NULL;
	r->version = version;
	r->federate = 1;
	r->replacetarget = NULL;

	list_init(&r->messages);

	*room = r;
	return 0;
}
void free_room(room_t *room)
{
	list_free(&room->messages, msg_t, entry, free_msg);

	if (room->replacetarget) {
		free(room->replacetarget->lasteventid);
		free(room->replacetarget->id);
		free(room->replacetarget);
	}
	free(room->version);
	free(room->creator);
	list_free(&room->powerlevels.users, powerlevel_t, entry, free_powerlevel);
	list_free(&room->powerlevels.events, powerlevel_t, entry, free_powerlevel);
	list_free(&room->members, member_t, entry, free_member);
	free(room->topic);
	free(room->name);
	free(room->id);
	free(room);
}

int apply_powerlevel_table(json_object *obj, listentry_t *levels)
{
	json_object_object_foreach(obj, key, val) {
		char *name = strdup(key);
		if (!name)
			return -1;

		powerlevel_t *level = malloc(sizeof(*level));
		if (!level) {
			free(name);
			return -1;
		}
		level->name = name;
		level->level = json_object_get_int(val);

		list_add(levels, &level->entry);
	}
	return 0;
}

/* state events */
int apply_room_create(json_object *obj, roomevent_meta_t *meta, room_t *room)
{
	free(room->creator);

	int err;
	char *creator = NULL;
	if ((err = get_object_as_string(obj, "creator", &creator))) {
		goto err_free;
	}

	char *version = NULL;
	if ((err = get_object_as_string(obj, "room_version", &version)) == -1) {
		if (err == -1)
			goto err_free;

		version = strdup("1");
		if (!version) {
			err = -1;
			goto err_free;
		}
	}

	int federate = -1;
	get_object_as_bool(obj, "m.federate", &federate);

	prevroom_t *prevroom = NULL;
	char *previd = NULL;
	char *preveventid = NULL;
	json_object *predecessor;
	json_object_object_get_ex(obj, "predecessor", &predecessor);
	if (prevroom) {
		if ((err = get_object_as_string(predecessor, "room_id", &previd)))
			goto err_free;
		if ((err = get_object_as_string(predecessor, "event_id", &preveventid)))
			goto err_free;
	}

	if (creator) {
		free(room->creator);
		room->creator = creator;
	}
	if (version) {
		free(room->version);
		room->version = version;
	}
	if (federate != -1)
		room->federate = federate;
	if (prevroom) {
		prevroom_t *prevroom = malloc(sizeof(*prevroom));
		if (!prevroom) {
			err = -1;
			goto err_free;
		}
		prevroom->id = previd;
		prevroom->lasteventid = preveventid;
		room->replacetarget = prevroom;
	}
	return 0;

err_free:
	free(preveventid);
	free(previd);
	free(prevroom);
	free(version);
	free(creator);
	return err;
}
int apply_room_member(json_object *obj, roomevent_meta_t *meta, room_t *room)
{
	member_t *member = malloc(sizeof(*member));
	if (!member)
		return -1;
	member->userid = NULL;
	member->membership = MEMBERSHIP_NUM;
	member->displayname = NULL;
	member->avatarurl = NULL;

	char *userid = strdup(meta->statekey);
	if (!userid)
		return -1;
	member->userid = userid;

	int err;
	if ((err = get_object_as_string(obj, "displayname", &member->displayname)) == -1)
		goto err_free;

	if ((err = get_object_as_enum(obj, "membership", (int *)&member->membership,
					MEMBERSHIP_NUM, membership_str))) {
		goto err_free;
	}

	if ((err = get_object_as_string(obj, "avatar_url", &member->avatarurl)) == -1)
		goto err_free;

	int isdirect = -1;
	if (get_object_as_bool(obj, "is_direct", &isdirect) == 0) {
		assert(0);
	}

	json_object *thirdinvite;
	json_object_object_get_ex(obj, "third_party_invite", &thirdinvite);
	if (thirdinvite) {
		assert(0);
	}

	json_object *usigned;
	json_object_object_get_ex(obj, "unsigned", &usigned);
	if (usigned) {
		assert(0);
	}

	int overwritten = 0;
	for (listentry_t *e = room->members.next; e != &room->members; e = e->next) {
		member_t *m = list_entry_content(e, member_t, entry);
		if (strcmp(m->userid, member->userid) == 0) {
			list_replace(&m->entry, &member->entry);
			free_member(m);
			overwritten = 1;
			break;
		}
	}
	if (!overwritten)
		list_add(&room->members, &member->entry);
	return 0;

err_free:
	free(member->avatarurl);
	free(member->displayname);
	free(member->userid);
	return err;
}
int apply_room_powerlevels(json_object *obj, roomevent_meta_t *meta, room_t *room)
{
	int err;
	get_object_as_int(obj, "invite", &room->powerlevels.invite);
	get_object_as_int(obj, "kick", &room->powerlevels.invite);
	get_object_as_int(obj, "ban", &room->powerlevels.invite);
	get_object_as_int(obj, "redact", &room->powerlevels.invite);

	get_object_as_int(obj, "state_default", &room->powerlevels.invite);
	get_object_as_int(obj, "events_default", &room->powerlevels.eventsdefault);

	json_object *events;
	json_object_object_get_ex(obj, "events", &events);
	if (events) {
		if ((err = apply_powerlevel_table(events, &room->powerlevels.events)))
			return err;
	}

	get_object_as_int(obj, "users_default", &room->powerlevels.usersdefault);

	json_object *users;
	json_object_object_get_ex(obj, "users", &users);
	if (users) {
		if ((err = apply_powerlevel_table(users, &room->powerlevels.users)))
			return err;
	}

	json_object *notif;
	json_object_object_get_ex(obj, "notifications", &notif);
	if (notif)
		get_object_as_int(obj, "room", &room->powerlevels.notif.room);

	return 0;
}
int apply_room_joinrules(json_object *obj, roomevent_meta_t *meta, room_t *room)
{
	int err;
	joinrule_t rule;
	if ((err = get_object_as_enum(obj, "join_rule", (int *)&room->joinrule,
					JOINRULE_NUM, joinrule_str)))
		return err;

	return 0;
}
int apply_room_history_visibility(json_object *obj, roomevent_meta_t *meta, room_t *room)
{
	int err;
	history_visibility_t visib;
	if ((err = get_object_as_enum(obj, "history_visibility", (int *)&room->histvisib,
					HISTVISIB_NUM, history_visibility_str)))
		return err;

	return 0;
}
int apply_room_name(json_object *obj, roomevent_meta_t *meta, room_t *room)
{
	int err;
	char *name;
	if ((err = get_object_as_string(obj, "name", &name)))
		return err;

	free(room->name);
	room->name = name;
	return 0;
}
int apply_room_topic(json_object *obj, roomevent_meta_t *meta, room_t *room)
{
	int err;
	char *topic;
	if ((err = get_object_as_string(obj, "topic", &topic)))
		return err;

	free(room->topic);
	room->topic = topic;
	return 0;
}

int apply_statevent(event_type_t type, roomevent_meta_t *meta, json_object *obj, room_t *room)
{
	int err;
	if ((err = get_object_as_string(obj, "state_key", &meta->statekey))) {
		return err;
	}

	json_object *content;
	json_object_object_get_ex(obj, "content", &content);
	if (!content) {
		free(meta->statekey);
		return 1;
	}

	switch (type) {
	case M_ROOM_CREATE:
		err = apply_room_create(content, meta, room);
		break;
	case M_ROOM_MEMBER:
		err = apply_room_member(content, meta, room);
		break;
	case M_ROOM_POWERLEVELS:
		err = apply_room_powerlevels(content, meta, room);
		break;
	case M_ROOM_JOINRULES:
		err = apply_room_joinrules(content, meta, room);
		break;
	case M_ROOM_HISTORY_VISIBILITY:
		err = apply_room_history_visibility(content, meta, room);
		break;
	case M_ROOM_NAME:
		err = apply_room_name(content, meta, room);
		break;
	case M_ROOM_TOPIC:
		err = apply_room_topic(content, meta, room);
		break;
	default:
		assert(0);
	}
	if (err) {
		free(meta->statekey);
		return err;
	}

	free(meta->statekey);
	return 0;
}

/* message events */
int get_room_message_text(json_object *obj, msg_text_t **msg)
{
	msg_text_t *m = malloc(sizeof(*m));
	if (!m)
		return -1;
	m->format = NULL;
	m->fmtbody = NULL;


	int err;
	if ((err = get_object_as_string(obj, "format", &m->format)) == -1) {
		free(m);
		return err;
	}

	if ((err = get_object_as_string(obj, "formatted_body", &m->fmtbody)) == -1) {
		free(m->format);
		free(m);
		return err;
	}

	*msg = m;
	return 0;
}

int apply_room_message(json_object *obj, roomevent_meta_t *meta, room_t *room)
{
	int err;
	msg_type_t type = MSG_NUM;
	if ((err = get_object_as_enum(obj, "msgtype", (int *)&type, MSG_NUM, msg_type_str)))
		return err;

	char *sender = strdup(meta->sender);
	if (!sender)
		return -1;

	char *body;
	if ((err = get_object_as_string(obj, "body", &body))) {
		free(sender);
		return err;
	}

	msg_t *m;
	switch (type) {
	case MSG_TEXT:;
		msg_text_t *text;
		if ((err = get_room_message_text(obj, &text)))
			goto err_free;
		m = &text->msg;
		break;
	default:
		assert(0);
	}

	m->type = type;
	m->sender = sender;
	m->body = body;
	list_add(&room->messages, &m->entry);
	return 0;

err_free:
	free(body);
	free(sender);
	return err;
}
int apply_message_event(event_type_t type, roomevent_meta_t *meta, json_object *obj, room_t *room)
{
	json_object *content;
	json_object_object_get_ex(obj, "content", &content);
	if (!content)
		return 1;

	int err;
	switch (type) {
	case M_ROOM_REDACTION:
		assert(0);
		break;
	case M_ROOM_MESSAGE:
		err = apply_room_message(content, meta, room);
		break;
	default:
		assert(0);
	}

	return err;
}
int apply_room_event(event_type_t type, json_object *obj, room_t *room, int stripped)
{
	roomevent_meta_t meta;
	meta.id = NULL;
	meta.transactid = NULL;

	int err = get_object_as_string(obj, "event_id", &meta.id);
	if (err == -1 || !stripped && err == 1)
		return err;
	
	if ((err = get_object_as_string(obj, "sender", &meta.sender))) {
		free(meta.id);
		return err;
	}

	int64_t ts = 0;
	err = get_object_as_int64(obj, "origin_server_ts", &ts);
	if (err == -1 || !stripped && err == 1) {
		free(meta.sender);
		free(meta.id);
		return err;
	}
	meta.ts = ts;

	json_object *usigned;
	json_object_object_get_ex(obj, "unsigned", &usigned);
	if (usigned) {
		int64_t age;
		if ((err = get_object_as_int64(usigned, "age", &age)) == -1) {
			free(meta.sender);
			free(meta.id);
			return err;
		}
		meta.age = age;

		if ((err = get_object_as_string(usigned, "transaction_id", &meta.transactid)) == -1) {
			free(meta.sender);
			free(meta.id);
			return err;
		}
	}

	if (type >= M_ROOM_CREATE && type <= M_ROOM_HISTORY_VISIBILITY) {
		err = apply_statevent(type, &meta, obj, room);
	} else if (type >= M_ROOM_REDACTION && type <= M_ROOM_MESSAGE) {
		err = apply_message_event(type, &meta, obj, room);
	} else {
		assert(0);
	}

	free(meta.transactid);
	free(meta.sender);
	free(meta.id);
	return err;
}

int apply_event(json_object *obj, room_t *room, int stripped)
{
	int err;
	event_type_t type = EVENT_TYPE_NUM;
	if ((err = get_object_as_enum(obj, "type", (int *)&type,
					EVENT_TYPE_NUM, event_type_strs))) {
		return err;
	}

	if (type >= M_ROOM_CREATE && type <= M_ROOM_MESSAGE) {
		err = apply_room_event(type, obj, room, stripped);
	} else {
		assert(0);
	}
	return err;
}
int apply_event_list(json_object *obj, room_t *room, int stripped)
{
	int err;
	size_t nevents = json_object_array_length(obj);
	for (size_t i = 0; i < nevents; ++i) {
		json_object *evobj = json_object_array_get_idx(obj, i);
		if ((err = apply_event(evobj, room, stripped)))
			return err;
	}
	return 0;
}

int apply_timeline(json_object *obj, room_t *room)
{
	int err;
	json_object *events;
	json_object_object_get_ex(obj, "events", &events);
	if (events) {
		if ((err = apply_event_list(events, room, 0)))
			return err;
	}
	return 0;
}
int apply_room_update_join(json_object *obj, room_t *room)
{
	int err;
	json_object *state;
	json_object_object_get_ex(obj, "state", &state);
	if (state) {
		if ((err = apply_timeline(state, room)))
			return err;
	}

	json_object *timeline;
	json_object_object_get_ex(obj, "timeline", &timeline);
	if (timeline) {
		if ((err = apply_timeline(timeline, room)))
			return err;
	}
	return 0;
}

int apply_invite_state(json_object *obj, room_t *room)
{
	int err;
	json_object *events;
	json_object_object_get_ex(obj, "events", &events);
	if (events) {
		if ((err = apply_event_list(events, room, 1)))
			return err;
	}
	return 0;
}
int apply_room_update_invite(json_object *obj, room_t *room)
{
	int err;
	json_object *state;
	json_object_object_get_ex(obj, "invite_state", &state);
	if (state) {
		if ((err = apply_invite_state(state, room)))
			return err;
	}
	return 0;
}
int __apply_room_updates(json_object *obj, listentry_t *rooms,
		int (*apply_room_update)(json_object *, room_t *))
{
	int err;
	json_object_object_foreach(obj, key, val) {
		room_t *target = NULL;
		for (listentry_t *e = rooms->next; e != rooms; e = e->next) {
			room_t *room = list_entry_content(e, room_t, entry);
			if (strcmp(room->id, key) == 0) {
				target = room;
				break;
			}
		}

		if (target == NULL) {
			if (get_new_room(key, &target))
				return -1;

			if ((err = apply_room_update(val, target))) {
				free_room(target);
				return err;
			}
			list_add(rooms, &target->entry);
		} else {
			if ((err = apply_room_update(val, target)))
				return err;
		}
	}
	return 0;
}
int apply_room_updates(json_object *obj, listentry_t *joinedrooms,
		listentry_t *invitedrooms, listentry_t *leftrooms)
{
	int err;
	json_object *join;
	json_object_object_get_ex(obj, "join", &join);
	if (join) {
		if ((err = __apply_room_updates(join, joinedrooms, apply_room_update_join)))
			return err;
	}

	json_object *invite;
	json_object_object_get_ex(obj, "invite", &invite);
	if (invite) {
		if ((err = __apply_room_updates(invite, invitedrooms, apply_room_update_invite)))
			return err;
	}

	//json_object *leave;
	//json_object_object_get_ex(obj, "leave", &leave);
	//if (leave) {
	//	assert(0);
	//}

	return 0;
}

int apply_sync_state_updates(json_object *obj, listentry_t *joinedrooms,
		listentry_t *invitedrooms, listentry_t *leftrooms, char **nextbatch)
{
	int err;
	json_object *roomupdates;
	json_object_object_get_ex(obj, "rooms", &roomupdates);
	if (roomupdates) {
		if ((err = apply_room_updates(roomupdates, joinedrooms, invitedrooms, leftrooms)))
			return err;
	}

	if ((err = get_object_as_string(obj, "next_batch", nextbatch))) {
		return err;
	}
	return 0;
}
