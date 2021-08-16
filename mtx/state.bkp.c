#include <stdio.h>

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "hjson.h"
#include "state.h"
#include "smc.h"

#define ROOMNAME_LEN_MAX 255

static void free_event(event_t *event);
static void free_event_list(listentry_t *head);
static int get_event(json_object *obj, event_t **event);

static void free_roominfo(roominfo_t *roominfo);
static int get_roominfo(json_object *obj, roominfo_type_t type, char *roomid, roominfo_t **roominfo);

int get_object_as_event(const json_object *obj, const char *key, event_t **event)
{
	int err;
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;

	event_t *e;
	if ((err = get_event(tmp, &e))) {
		return err;
	}

	*event = e;
	return 0;
}
int get_object_as_event_list(const json_object *obj, const char *key, listentry_t **head)
{
	int err;
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;

	listentry_t *h = malloc(sizeof(*h));
	list_init(h);
	size_t nevents = json_object_array_length(tmp);
	for (size_t i = 0; i < nevents; ++i) {
		json_object *eventobj = json_object_array_get_idx(tmp, i);

		event_t *event;
		if ((err = get_event(eventobj, &event)))
			goto err_free;

		list_add(h, &event->entry);
	}
	*head = h;
	return 0;

err_free:;
	listentry_t *e = h->next;
	while (e != h) {
		listentry_t *next = e->next;
		event_t *ev = list_entry_content(e, event_t, entry);
		free_event(ev);
		e = next;
	}
	free(h);
	return err;
}
int get_object_as_roominfo_list(const json_object *obj, const char *key,
		roominfo_type_t type, listentry_t **head)
{
	int err = 0;
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;

	listentry_t *h = malloc(sizeof(*h));
	list_init(h);
	json_object_object_foreach(tmp, k, v) {
		char *key = strdup(k);
		if (!key) {
			err = -1;
			goto err_free;
		}

		roominfo_t *info;
		if ((err = get_roominfo(v, type, key, &info)))
			goto err_free;

		printf("%s\n", info->id);
		list_add(h, &info->entry);
	}
	*head = h;
	printf("\n");
	return 0;

err_free:;
	listentry_t *e = h->next;
	while (e != h) {
		listentry_t *next = e->next;
		roominfo_t *info = list_entry_content(e, roominfo_t, entry);
		free_roominfo(info);
		e = next;
	}
	free(h);
	return err;
}

int is_statevent_type(event_type_t type)
{
	if (type >= STATEVENT_FIRST && type <= STATEVENT_LAST) {
		return 1;
	}
	return 0;
}


static void free_room_create(roomevent_create_t *create)
{
	free(create->prevroom.eventid);
	free(create->prevroom.roomid);
	free(create->creator);
	free(create);
}
static void free_room_member(roomevent_member_t *member)
{
	free_event_list(member->invite_room_state.events);
	free(member->thirdparty_invite.displayname);
	free(member->displayname);
	free(member->avatarurl);
	free(member);
}
static void free_room_joinrules(roomevent_joinrules_t *joinrules)
{
	free(joinrules);
}
static void free_room_powerlevels(roomevent_powerlevels_t *powerlevels)
{
	for (size_t i = 0; i < powerlevels->events.num; ++i) {
		free(powerlevels->events.names);
	}
	free(powerlevels->events.names);
	free(powerlevels->events.levels);

	for (size_t i = 0; i < powerlevels->users.num; ++i) {
		free(powerlevels->users.names);
		free(powerlevels->users.levels);
	}
	free(powerlevels->users.names);
	free(powerlevels);
}
static void free_room_canonical_alias(roomevent_canonical_alias_t *canonalias)
{
	assert(0);
}
static void free_room_redaction(roomevent_redaction_t *redaction)
{
	assert(0);
}
static void free_statevent(statevent_t *event)
{
	free_event(event->prevcontent);
	free(event->statekey);

	switch (event->revent.event.type) {
	case M_ROOM_CREATE:
		free_room_create(CONTAINER(event, roomevent_create_t, sevent));
		break;
	case M_ROOM_MEMBER:
		free_room_create(CONTAINER(event, roomevent_create_t, sevent));
		break;
	case M_ROOM_JOINRULES:
		free_room_create(CONTAINER(event, roomevent_create_t, sevent));
		break;
	case M_ROOM_POWERLEVELS:
		free_room_create(CONTAINER(event, roomevent_create_t, sevent));
		break;
	case M_ROOM_CANONICAL_ALIAS:
		free_room_create(CONTAINER(event, roomevent_create_t, sevent));
		break;
	default:
		assert(0);
	}

}
static void free_roomevent(roomevent_t *event)
{
	free(event->transactid);
	free_event(event->redactreason);
	free(event->redactreason);
	free(event->sender);
	free(event->id);

	if (event->event.type >= M_ROOM_CREATE && event->event.type <= M_ROOM_CANONICAL_ALIAS) {
		free_statevent(CONTAINER(event, statevent_t, revent));
	} else if (event->event.type == M_ROOM_REDACTION) {
		free_room_redaction(CONTAINER(event, roomevent_redaction_t, revent));
	} else {
		assert(0);
	}
}
static void free_event(event_t *event)
{
	if (!event)
		return;

	if (event->type >= M_ROOM_CREATE && event->type <= M_ROOM_CANONICAL_ALIAS) {
		free_roomevent(CONTAINER(event, roomevent_t, event));
	} else {
		assert(0);
	}
}
static void free_event_list(listentry_t *head)
{
	if (!head)
		return;

	listentry_t *e = head->next;
	while (e != head) {
		listentry_t *next = e->next;
		event_t *ev = list_entry_content(e, event_t, entry);
		free_event(ev);
		e = next;
	}
	free(head);
}

static int get_room_create(json_object *obj, roomevent_create_t **create)
{
	roomevent_create_t *ev = malloc(sizeof(*ev));
	if (!ev)
		return -1;
	ev->federate = 1;
	ev->roomversion = NULL;
	ev->prevroom.roomid = NULL;
	ev->prevroom.eventid = NULL;

	int err;
	if ((err = get_object_as_string(obj, "creator", &ev->creator))) {
		goto err_free;
	}

	if ((err = get_object_as_string(obj, "room_version", &ev->roomversion) == -1)) {
		if (err == -1)
			goto err_free;

		char *version = strdup("1");
		if (!version) {
			err = -1;
			goto err_free;
		}
		ev->roomversion = version;
	}

	get_object_as_bool(obj, "m.federate", &ev->federate);

	json_object *prevroom;
	json_object_object_get_ex(obj, "predecessor", &prevroom);
	if (prevroom) {
		if ((err = get_object_as_string(prevroom, "room_id", &ev->prevroom.roomid)))
			goto err_free;
		if ((err = get_object_as_string(prevroom, "event_id", &ev->prevroom.eventid)))
			goto err_free;
	}

	*create = ev;
	return 0;

err_free:
	free(ev->prevroom.eventid);
	free(ev->prevroom.roomid);
	free(ev->creator);
	free(ev);
	return err;
}
static int get_room_member(json_object *obj, roomevent_member_t **member)
{
	roomevent_member_t *ev = malloc(sizeof(*ev));
	if (!ev)
		return -1;
	ev->avatarurl = NULL;
	ev->displayname = NULL;
	ev->isdirect = -1;
	ev->thirdparty_invite.displayname = NULL;
	ev->invite_room_state.events = NULL;

	int err;
	if ((err = get_object_as_string(obj, "avatar_url", &ev->avatarurl)) == -1)
		goto err_free;

	if ((err = get_object_as_string(obj, "displayname", &ev->displayname)) == -1)
		goto err_free;

	int membership;
	if ((err = get_object_as_enum(obj, "membership", &membership,
					MEMBERSHIP_NUM, membership_str))) {
		goto err_free;
	}
	ev->membership = (membership_t)membership;

	get_object_as_bool(obj, "is_direct", &ev->isdirect);

	json_object *thirdparty_invite;
	json_object_object_get_ex(obj, "third_party_invite", &thirdparty_invite);
	ev->thirdparty_invite.displayname = NULL;
	if (thirdparty_invite) {
		if ((err = get_object_as_string(thirdparty_invite, "display_name",
						&ev->thirdparty_invite.displayname))) {
			goto err_free;
		}
	}

	json_object *usigned;
	json_object_object_get_ex(obj, "unsigned", &usigned);
	if (usigned) {
		if ((err = get_object_as_event_list(usigned, "invite_room_state",
						&ev->invite_room_state.events)) == -1) {
			goto err_free;
		}
	}

	*member = ev;
	return 0;

err_free:
	free_event_list(ev->invite_room_state.events);
	free(ev->thirdparty_invite.displayname);
	free(ev->displayname);
	free(ev->avatarurl);
	free(ev);
	return err;
}
static int get_room_joinrules(json_object *obj, roomevent_joinrules_t **joinrules)
{
	roomevent_joinrules_t *ev = malloc(sizeof(*ev));
	if (!ev)
		return -1;

	int err;
	int rule;
	if ((err = get_object_as_enum(obj, "join_rule", &rule, JOINRULES_NUM, joinrule_str))) {
		return err;
	}
	ev->rule = rule;

	*joinrules = ev;
	return 0;
}
static int get_room_powerlevels(json_object *obj, roomevent_powerlevels_t **powerlevels)
{
	roomevent_powerlevels_t *ev = malloc(sizeof(*ev));
	if (!ev)
		return -1;
	ev->invite = 50;
	ev->kick = 50;
	ev->ban = 50;
	ev->redact = 50;
	ev->statedefault = 50;

	ev->events.num = 0;
	ev->events.names = NULL;
	ev->events.levels = NULL;

	ev->users.num = 0;
	ev->users.names = NULL;
	ev->users.levels = NULL;

	ev->usersdefault = 0;
	ev->notifications.room = 50;

	get_object_as_int(obj, "invite", &ev->invite);
	get_object_as_int(obj, "kick", &ev->kick);
	get_object_as_int(obj, "ban", &ev->ban);
	get_object_as_int(obj, "redact", &ev->redact);
	get_object_as_int(obj, "state_default", &ev->statedefault);

	int err;
	if ((err = get_object_as_int_table(obj, "events", &ev->events.names,
					&ev->events.levels)) == -1) {
		goto err_free;
	}
	get_object_as_int(obj, "events_default", &ev->eventsdefault);

	if ((err = get_object_as_int_table(obj, "users", &ev->users.names,
					&ev->users.levels)) == -1) {
		goto err_free;
	}
	get_object_as_int(obj, "users_default", &ev->usersdefault);

	json_object *notif;
	json_object_object_get_ex(obj, "notifications", &notif);
	if (notif)
		get_object_as_int(notif, "room", &ev->notifications.room);

	*powerlevels = ev;
	return 0;

err_free:
	free(ev->users.names);
	for (size_t i = 0; i < ev->users.num; ++i) {
		free(ev->users.names[i]);
	}
	free(ev->users.names);
	free(ev->users.levels);

	for (size_t i = 0; i < ev->events.num; ++i) {
		free(ev->events.names[i]);
	}
	free(ev->events.names);
	free(ev->events.levels);
	free(ev);
	return err;
}
static int get_room_canonical_alias(json_object *obj, roomevent_canonical_alias_t **canonalias)
{
	roomevent_canonical_alias_t *ev = malloc(sizeof(*ev));
	if (!ev)
		return 1;
	ev->alias = NULL;
	ev->altaliases = NULL;

	int err;
	if ((err = get_object_as_string(obj, "alias", &ev->alias)) == -1) {
		return err;
	}

	if ((err = get_object_as_string_array(obj, "alt_aliases",
					&ev->naltaliases, &ev->altaliases)) == -1) {
		free(ev->alias);
		return err;
	}

	*canonalias = ev;
	return 0;
}
static int get_room_redaction(json_object *obj, roomevent_redaction_t **redaction)
{
	roomevent_redaction_t *ev = malloc(sizeof(*ev));
	if (!ev)
		return 1;
	ev->reason = NULL;

	int err;
	if ((err = get_object_as_string(obj, "reason", &ev->reason)) == -1) {
		return err;
	}

	*redaction = ev;
	return 0;
}

static int get_room_name(json_object *obj, roomevent_name_t **name)
{
	roomevent_name_t *ev = malloc(sizeof(*ev));
	if (!ev)
		return 1;

	int err;
	if ((err = get_object_as_string(obj, "name", &ev->name)))
		return err;

	if (strlen(ev->name) > ROOMNAME_LEN_MAX) {
		free(ev->name);
		return 1;
	}

	*name = ev;
	return 0;
}
static int get_room_topic(json_object *obj, roomevent_topic_t **topic)
{
	roomevent_topic_t *ev = malloc(sizeof(*ev));
	if (!ev)
		return 1;

	int err;
	if ((err = get_object_as_string(obj, "topic", &ev->topic)))
		return err;

	*topic = ev;
	return 0;
}

static int get_room_history_visibility(json_object *obj, roomevent_history_visibility_t **histvisib)
{
	roomevent_history_visibility_t *ev = malloc(sizeof(*ev));
	if (!ev)
		return 1;

	int err;
	int visib;
	if ((err = get_object_as_enum(obj, "history_visibility", &visib,
					HISTVISIB_NUM, history_visibility_str)))
		return err;
	ev->visib = visib;

	*histvisib = ev;
	return 0;
}

static int get_statevent(json_object *obj, statevent_t **event)
{
	statevent_t ev;
	ev.prevcontent = NULL;

	int err;
	int type;
	if ((err = get_object_as_enum(obj, "type", &type, EVENT_TYPE_NUM, event_type_strs)))
		return err;

	if ((err = get_object_as_string(obj, "state_key", &ev.statekey)))
		return err;

	if ((err = get_object_as_event(obj, "prev_content", &ev.prevcontent)) == -1)
		goto err_free;

	json_object *content;
	json_object_object_get_ex(obj, "content", &content);
	if (!content) {
		err = 1;
		goto err_free;
	}

	switch (type) {
	case M_ROOM_CREATE:;
		roomevent_create_t *create;
		if ((err = get_room_create(content, &create)))
			goto err_free;
		memcpy(&create->sevent, &ev, sizeof(ev));
		*event = &create->sevent;
		break;
	case M_ROOM_MEMBER:;
		roomevent_member_t *member;
		if ((err = get_room_member(content, &member)))
			goto err_free;
		memcpy(&member->sevent, &ev, sizeof(ev));
		*event = &member->sevent;
		break;
	case M_ROOM_JOINRULES:;
		roomevent_joinrules_t *joinrules;
		if ((err = get_room_joinrules(content, &joinrules)))
			goto err_free;
		memcpy(&joinrules->sevent, &ev, sizeof(ev));
		*event = &joinrules->sevent;
		break;
	case M_ROOM_POWERLEVELS:;
		roomevent_powerlevels_t *powerlevels;
		if ((err = get_room_powerlevels(content, &powerlevels)))
			goto err_free;
		memcpy(&powerlevels->sevent, &ev, sizeof(ev));
		*event = &powerlevels->sevent;
		break;
	case M_ROOM_CANONICAL_ALIAS:;
		roomevent_canonical_alias_t *canonalias;
		if ((err = get_room_canonical_alias(content, &canonalias)))
			goto err_free;
		memcpy(&canonalias->sevent, &ev, sizeof(ev));
		*event = &canonalias->sevent;
		break;
	case M_ROOM_NAME:;
		roomevent_name_t *name;
		if ((err = get_room_name(content, &name)))
			goto err_free;
		memcpy(&name->sevent, &ev, sizeof(ev));
		*event = &name->sevent;
		break;
	case M_ROOM_TOPIC:;
		roomevent_topic_t *topic;
		if ((err = get_room_topic(content, &topic)))
			goto err_free;
		memcpy(&topic->sevent, &ev, sizeof(ev));
		*event = &topic->sevent;
		break;
	case M_ROOM_HISTORY_VISIBILITY:;
		roomevent_history_visibility_t *histvisib;
		if ((err = get_room_history_visibility(content, &histvisib)))
			goto err_free;
		memcpy(&histvisib->sevent, &ev, sizeof(ev));
		*event = &histvisib->sevent;
		break;
	default:
		assert(0);
	}
	return 0;

err_free:
	free(ev.prevcontent);
	free(ev.statekey);
	return err;
}
static int get_roomevent(json_object *obj, roomevent_t **event)
{
	roomevent_t ev;
	ev.id = NULL;
	ev.sender = NULL;
	ev.age = -1;
	ev.redactreason = NULL;
	ev.transactid = NULL;

	int err;
	int type;
	if ((err = get_object_as_enum(obj, "type", &type, EVENT_TYPE_NUM, event_type_strs)))
		return err;

	if ((err = get_object_as_string(obj, "event_id", &ev.id)))
		return err;

	if ((err = get_object_as_string(obj, "sender", &ev.sender)))
		goto err_free;

	int64_t ts;
	if ((err = get_object_as_int64(obj, "origin_server_ts", &ts)))
		goto err_free;
	ev.ts = (long)ts;

	json_object *extradata;
	json_object_object_get_ex(obj, "unsigned", &extradata);
	if (extradata) {
		int64_t age;
		get_object_as_int64(extradata, "age", &age);
		ev.age = age;

		if (get_object_as_event(extradata, "redacted_because", &ev.redactreason) == -1)
			goto err_free;

		if (get_object_as_string(extradata, "transaction_id", &ev.transactid) == -1)
			goto err_free;
	}

	json_object *content;
	json_object_object_get_ex(obj, "content", &content);
	if (!content) {
		err = 1;
		goto err_free;
	}

	if (is_statevent_type(type)) {
		statevent_t *sevent;
		if ((err = get_statevent(obj, &sevent)))
			goto err_free;
		memcpy(&sevent->revent, &ev, sizeof(ev));
		*event = &sevent->revent;
	} else if (type == M_ROOM_REDACTION) {
		roomevent_redaction_t *redaction;
		if ((err = get_room_redaction(content, &redaction)))
			goto err_free;
		memcpy(&redaction->revent, &ev, sizeof(ev));
		*event = &redaction->revent;
	}
	return 0;

err_free:
	free(ev.transactid);
	free_event(ev.redactreason);
	free(ev.redactreason);
	free(ev.sender);
	free(ev.id);
	return err;
}
static int get_event(json_object *obj, event_t **event)
{
	event_t ev;

	int err;
	int type;
	if ((err = get_object_as_enum(obj, "type", &type, EVENT_TYPE_NUM, event_type_strs)))
		return err;
	ev.type = (event_type_t)type;

	if (ev.type >= M_ROOM_CREATE && ev.type <= M_ROOM_REDACTION) {
		roomevent_t *revent;
		if ((err = get_roomevent(obj, &revent)))
			return err;
		memcpy(&revent->event, &ev, sizeof(ev));
		*event = &revent->event;
	} else {
		assert(0);
	}
	return 0;
}

static void free_roomsummary(roomsummary_t *summary)
{
	for (size_t i = 0; i < summary->nheroes; ++i) {
		free(summary->heroes[i]);
	}
	free(summary->heroes);
}
static void free_roomstate(roomstate_t *roomstate)
{
	free_event_list(roomstate->events);
}
static void free_timeline(timeline_t *timeline)
{
	free(timeline->prevbatch);
	free_event_list(timeline->events);
}
static void free_ephemeral(epheremal_t *ephstate)
{
	free_event_list(ephstate->events);
}
static void free_account_data(accountstate_t *accountstate)
{
	free_event_list(accountstate->events);
}
static void free_roominfo_joined(roominfo_joined_t *roominfo)
{
	free_account_data(&roominfo->accountstate);
	free_ephemeral(&roominfo->ephstate);
	free_timeline(&roominfo->timeline);
	free_roomstate(&roominfo->roomstate);
	free_roomsummary(&roominfo->summary);
}
static void free_roominfo_invited(roominfo_invited_t *roominfo)
{
	free_roomstate(&roominfo->state);
}
static void free_roominfo_left(roominfo_left_t *roominfo)
{
	free_account_data(&roominfo->accountdata);
	free_timeline(&roominfo->timeline);
	free_event_list(roominfo->state.events);
}
static void free_roominfo(roominfo_t *roominfo)
{
	if (!roominfo)
		return;

	switch(roominfo->type) {
	case ROOMINFO_JOINED:
		free_roominfo_joined(CONTAINER(roominfo, roominfo_joined_t, rinfo));
		return;
	case ROOMINFO_INVITED:
		free_roominfo_invited(CONTAINER(roominfo, roominfo_invited_t, rinfo));
		return;
	case ROOMINFO_LEFT:
		free_roominfo_left(CONTAINER(roominfo, roominfo_left_t, rinfo));
		return;
	default:
		assert(0);
	}
}
static void free_roominfo_list(listentry_t *head)
{
	if (!head)
		return;

	listentry_t *e = head->next;
	while (e != head) {
		listentry_t *next = e->next;
		roominfo_t *info = list_entry_content(e, roominfo_t, entry);
		free_roominfo(info);
		e = next;
	}
	free(head);
}

static int get_roomsummary(json_object *obj, roomsummary_t *summary)
{
	summary->nheroes = 0;
	summary->heroes = NULL;
	summary->ninvited = 0;
	summary->njoined = 0;

	int err;
	if ((err = get_object_as_string_array(obj, "m.heroes",
					&summary->nheroes, &summary->heroes)) == -1)
		goto err_free;

	int32_t njoined;
	get_object_as_int(obj, "m.joined_member_count", &njoined);
	summary->njoined = njoined;

	int32_t ninvited;
	get_object_as_int(obj, "m.invited_member_count", &ninvited);
	summary->ninvited = ninvited;
	return 0;

err_free:
	for (size_t i = 0; i < summary->nheroes; ++i) {
		free(summary->heroes);
	}
	return err;
}
static int get_roomstate(json_object *obj, roomstate_t *state)
{
	state->events = NULL;
	return get_object_as_event_list(obj, "events", &state->events);
}
static int get_timeline(json_object *obj, timeline_t *timeline)
{
	timeline->events = NULL;
	timeline->prevbatch = NULL;

	int err;
	if ((err = get_object_as_event_list(obj, "events", &timeline->events)) == -1)
		return -1;

	get_object_as_bool(obj, "limited", &timeline->limited);

	if ((err = get_object_as_string(obj, "prev_batch", &timeline->prevbatch))) {
		free_event_list(timeline->events);
		return -1;
	}
	return 0;
}
static int get_ephemeral(json_object *obj, epheremal_t *ephemeral)
{
	ephemeral->events = NULL;
	return get_object_as_event_list(obj, "events", &ephemeral->events);
}
static int get_account_data(json_object *obj, accountstate_t *accountstate)
{
	accountstate->events = NULL;
	return get_object_as_event_list(obj, "events", &accountstate->events);
}
static int get_roominfo_joined(json_object *obj, roominfo_joined_t **roominfo)
{
	roominfo_joined_t *info = malloc(sizeof(*info));
	if (!info)
		return -1;

	int err;
	json_object *tmp;
	json_object_object_get_ex(obj, "summary", &tmp);
	if (tmp) {
		if ((err = get_roomsummary(tmp, &info->summary)) == -1) {
			free(info);
			return -1;
		}
	}

	json_object_object_get_ex(obj, "state", &tmp);
	if (tmp) {
		if ((err = get_roomstate(tmp, &info->roomstate)) == -1) {
			free_roomsummary(&info->summary);
			free(info);
			return -1;
		}
	}

	json_object_object_get_ex(obj, "timeline", &tmp);
	if (tmp) {
		if ((err = get_timeline(tmp, &info->timeline)) == -1) {
			free_roomstate(&info->roomstate);
			free_roomsummary(&info->summary);
			free(info);
			return -1;
		}
	}

	json_object_object_get_ex(obj, "ephemeral", &tmp);
	if (tmp) {
		if ((err = get_ephemeral(tmp, &info->ephstate)) == -1) {
			free_timeline(&info->timeline);
			free_roomstate(&info->roomstate);
			free_roomsummary(&info->summary);
			free(info);
			return -1;
		}
	}

	json_object_object_get_ex(obj, "account_data", &tmp);
	if (tmp) {
		if ((err = get_account_data(tmp, &info->accountstate))) {
			free_ephemeral(&info->ephstate);
			free_timeline(&info->timeline);
			free_roomstate(&info->roomstate);
			free_roomsummary(&info->summary);
			free(info);
			return -1;
		}
	}

	json_object_object_get_ex(obj, "unread_notifications", &tmp);
	if (tmp) {
		int32_t hlnotenum;
		get_object_as_int(tmp, "highlight_count", &hlnotenum);
		info->hlnotenum = hlnotenum;

		int32_t notenum;
		get_object_as_int(tmp, "notification_count", &notenum);
		info->notenum = notenum;
	}

	*roominfo = info;
	return 0;
}
static int get_roominfo_invited(json_object *obj, roominfo_invited_t **roominfo)
{
	roominfo_invited_t *info = malloc(sizeof(*info));
	if (!info)
		return -1;

	int err;
	json_object *tmp;
	json_object_object_get_ex(obj, "invite_state", &tmp);
	if (tmp) {
		if ((err = get_roomstate(tmp, &info->state)) == -1) {
			free(info);
			return -1;
		}
	}

	*roominfo = info;
	return 0;
}
static int get_roominfo_left(json_object *obj, roominfo_left_t **roominfo)
{
	roominfo_left_t *info = malloc(sizeof(*info));
	info->state.events = NULL;

	int err;
	json_object *tmp;
	json_object_object_get_ex(obj, "state", &tmp);
	if (tmp) {
		if ((err = get_roomstate(obj, &info->state)) == -1) {
			free(info);
			return -1;
		}
	}

	json_object_object_get_ex(obj, "timeline", &tmp);
	if (tmp) {
		if ((err = get_timeline(tmp, &info->timeline)) == -1) {
			free_event_list(info->state.events);
			free(info);
			return -1;
		}
	}

	json_object_object_get_ex(obj, "account_data", &tmp);
	if (tmp) {
		if ((err = get_account_data(tmp, &info->accountdata)) == -1) {
			free_timeline(&info->timeline);
			free_event_list(info->accountdata.events);
			free(info);
			return -1;
		}
	}
	
	*roominfo = info;
	return 0;
}
static int get_roominfo(json_object *obj, roominfo_type_t type, char *roomid, roominfo_t **roominfo)
{
	roominfo_t info;
	info.type = type;

	char *id = strdup(roomid);
	if (!id)
		return -1;
	info.id = id;

	int err;
	switch (type) {
	case ROOMINFO_JOINED:;
		roominfo_joined_t *joined;
		if ((err = get_roominfo_joined(obj, &joined)))
			goto err_free;
		memcpy(&joined->rinfo, &info, sizeof(info));
		*roominfo = &joined->rinfo;
		break;
	case ROOMINFO_INVITED:;
		roominfo_invited_t *invited;
		if ((err = get_roominfo_invited(obj, &invited)))
			goto err_free;
		memcpy(&invited->rinfo, &info, sizeof(info));
		*roominfo = &invited->rinfo;
		break;
	case ROOMINFO_LEFT:;
		roominfo_left_t *left;
		if ((err = get_roominfo_left(obj, &left)))
			goto err_free;
		memcpy(&left->rinfo, &info, sizeof(info));
		*roominfo = &left->rinfo;
		break;
	default:
		assert(0);
	}
	return 0;

err_free:
	free(info.id);
	return err;
}

static int get_rooms(json_object *obj, listentry_t **roominfos)
{
	int err;
	listentry_t *joined;
	if ((err = get_object_as_roominfo_list(obj, "join", ROOMINFO_JOINED, &joined)) == -1) {
		return -1;
	}

	listentry_t *invited;
	if ((err = get_object_as_roominfo_list(obj, "invite", ROOMINFO_INVITED, &invited)) == -1) {
		free_roominfo_list(joined);
		return -1;
	}

	listentry_t *left;
	if ((err = get_object_as_roominfo_list(obj, "invite", ROOMINFO_LEFT, &left)) == -1) {
		free_roominfo_list(invited);
		free_roominfo_list(joined);
		return -1;
	}

	listentry_t *infos = joined;
	list_concat(infos, invited);
	list_concat(infos, left);
	free(invited);
	free(left);

	*roominfos = infos;
	return 0;
}

void free_state(state_t *state)
{
	if (!state)
		return;

	free_roominfo_list(state->rooms);
	free(state->next_batch);
}
int parse_state(json_object *obj, state_t **state)
{
	state_t *s = malloc(sizeof(*s));
	if (!s)
		return -1;
	s->next_batch = NULL;

	int err;
	if ((err = get_object_as_string(obj, "next_batch", &s->next_batch)) == -1)
		return err;

	json_object *tmp;
	json_object_object_get_ex(obj, "rooms", &tmp);
	if (tmp) {
		if ((err = get_rooms(tmp, &s->rooms))) {
			free(s->next_batch);
			return err;
		}
	}

	for (listentry_t *e = s->rooms->next; e != s->rooms; e = e->next) {
		roominfo_t *info = list_entry_content(e, roominfo_t, entry);
		printf("%s\n", info->id);
	};

	*state = s;
	return 0;
}
