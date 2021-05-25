#include <assert.h>
#include <json-c/json.h>
#include <string.h>

#include "state.h"
#include "hjson.h"

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
};

void free_member(member_t *member)
{
	free(member->name);
	free(member->avatarurl);
	free(member);
}
void free_powerlevel(powerlevel_t *powerlevel)
{
	free(powerlevel->name);
	free(powerlevel);
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

	*room = r;
	return 0;
}
void free_room(room_t *room)
{
	if (room->replacetarget) {
		free(room->replacetarget->lasteventid);
		free(room->replacetarget->id);
		free(room->replacetarget);
	}
	free(room->version);
	free(room->creator);
	LIST_FREE(&room->powerlevels.users, powerlevel_t, entry, free_powerlevel);
	LIST_FREE(&room->powerlevels.events, powerlevel_t, entry, free_powerlevel);
	LIST_FREE(&room->members, member_t, entry, free_member);
	free(room->topic);
	free(room->name);
	free(room->id);
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

int apply_room_create(json_object *obj, room_t *room)
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
int apply_room_member(json_object *obj, room_t *room)
{
	member_t *member = malloc(sizeof(*member));
	if (!member)
		return -1;
	member->name = NULL;
	member->avatarurl = NULL;
	member->membership = MEMBERSHIP_NUM;

	int err;

	if ((err = get_object_as_string(obj, "displayname", &member->name)) == -1)
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

	list_add(&room->members, &member->entry);
	return 0;

err_free:
	free(member->avatarurl);
	free(member->name);
	return err;
}
int apply_room_powerlevels(json_object *obj, room_t *room)
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
int apply_room_joinrules(json_object *obj, room_t *room)
{
	int err;
	joinrule_t rule;
	if ((err = get_object_as_enum(obj, "join_rule", (int *)&room->joinrule,
					JOINRULE_NUM, joinrule_str)))
		return err;

	return 0;
}
int apply_room_history_visibility(json_object *obj, room_t *room)
{
	int err;
	history_visibility_t visib;
	if ((err = get_object_as_enum(obj, "history_visibility", (int *)&room->histvisib,
					HISTVISIB_NUM, history_visibility_str)))
		return err;

	return 0;
}
int apply_room_name(json_object *obj, room_t *room)
{
	int err;
	char *name;
	if ((err = get_object_as_string(obj, "name", &name)))
		return err;

	free(room->name);
	room->name = name;
	return 0;
}
int apply_room_topic(json_object *obj, room_t *room)
{
	int err;
	char *topic;
	if ((err = get_object_as_string(obj, "topic", &topic)))
		return err;

	free(room->topic);
	room->topic = topic;
	return 0;
}

int apply_statevent(json_object *obj, room_t *room)
{
	int err;
	event_type_t type = EVENT_TYPE_NUM;
	if ((err = get_object_as_enum(obj, "type", (int *)&type, EVENT_TYPE_NUM, event_type_strs))) {
		return err;
	}

	json_object *content;
	json_object_object_get_ex(obj, "content", &content);
	if (!content)
		return err;

	switch (type) {
	case M_ROOM_CREATE:
		apply_room_create(content, room);
		break;
	case M_ROOM_MEMBER:
		apply_room_member(content, room);
		break;
	case M_ROOM_POWERLEVELS:
		apply_room_powerlevels(content, room);
		break;
	case M_ROOM_JOINRULES:
		apply_room_joinrules(content, room);
		break;
	case M_ROOM_HISTORY_VISIBILITY:
		apply_room_history_visibility(content, room);
		break;
	case M_ROOM_NAME:
		apply_room_name(content, room);
		break;
	case M_ROOM_TOPIC:
		apply_room_topic(content, room);
		break;
	default:
		assert(0);
	}
	return 0;
}
int apply_event_list(json_object *obj, room_t *room)
{
	int err;
	size_t nevents = json_object_array_length(obj);
	for (size_t i = 0; i < nevents; ++i) {
		json_object *evobj = json_object_array_get_idx(obj, i);
		if ((err = apply_statevent(evobj, room)))
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
		if ((err = apply_event_list(events, room)))
			return err;
	}
	return 0;
}

int apply_room_update_join(json_object *obj, room_t *room)
{
	int err;
	json_object *timeline;
	json_object_object_get_ex(obj, "timeline", &timeline);
	if (timeline) {
		if ((err = apply_timeline(timeline, room)))
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
			room_t *room = LIST_ENTRY(e, room_t, entry);
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

	//json_object *invite;
	//json_object_object_get_ex(obj, "invite", &invite);
	//if (invite) {
	//	assert(0);
	//}

	//json_object *leave;
	//json_object_object_get_ex(obj, "leave", &leave);
	//if (leave) {
	//	assert(0);
	//}

	return 0;
}

int apply_state_updates(json_object *obj, listentry_t *joinedrooms,
		listentry_t *invitedrooms, listentry_t *leftrooms)
{
	int err;
	json_object *roomupdates;
	json_object_object_get_ex(obj, "rooms", &roomupdates);
	if (roomupdates) {
		if ((err = apply_room_updates(roomupdates, joinedrooms, invitedrooms, leftrooms)))
			return err;
	}

	return 0;
}
