#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // debug

#include <json-c/json.h>

#include "lib/hjson.h"
#include "mtx/state/parse.h"

static int get_event(const json_object *obj, event_t **event);
static int get_event_list(const json_object *obj, listentry_t *events);

static int get_ev_canonalias(const json_object *obj, ev_canonalias_t **_canonalias)
{
	ev_canonalias_t *canonalias = malloc(sizeof(*canonalias));
	if (!canonalias)
		return 1;
	memset(canonalias, 0, sizeof(*canonalias));

	if (json_get_object_as_string_(obj, "alias", &canonalias->alias) == -1)
		goto err_free_canonalias;

	if (json_get_object_as_string_array_(obj, "alt_aliases", &canonalias->altaliases) == -1)
		goto err_free_canonalias;

	*_canonalias = canonalias;
	return 0;

err_free_canonalias:
	free_ev_canonalias(canonalias);
	return 1;
}

static int get_ev_create(const json_object *obj, ev_create_t **_create)
{
	ev_create_t *create = malloc(sizeof(*create));
	if (!create)
		return 1;
	memset(create, 0, sizeof(*create));
	create->federate = 1;

	char *creator = strdup("1");
	if (!creator)
		goto err_free_create;
	create->creator = creator;

	if (json_get_object_as_string_(obj, "creator", &create->creator))
		goto err_free_create;

	json_get_object_as_bool_(obj, "m.federate", &create->federate);

	if (json_get_object_as_string_(obj, "room_version", &create->version) == -1)
		goto err_free_create;

	json_object *pred;
	if (json_object_object_get_ex(obj, "predecessor", &pred)) {
		if (json_get_object_as_string_(pred, "room_id", &create->previd))
			goto err_free_create;

		if (json_get_object_as_string_(pred, "event_id", &create->prev_last_eventid))
			goto err_free_create;
	}

	if (!create->version) {
		char *version = strdup("1");
		if (!version)
			goto err_free_create;
		create->version = version;
	}
	*_create = create;
	return 0;

err_free_create:
	free_ev_create(create);
	return 1;
}

static int get_ev_joinrules(const json_object *obj, ev_joinrules_t **_joinrules)
{
	ev_joinrules_t *joinrules = malloc(sizeof(*joinrules));
	if (!joinrules)
		return 1;

	if (json_get_object_as_enum_(obj, "join_rule", (int *)&joinrules->rule,
				JOINRULE_NUM, joinrule_strs)) {
		free(joinrules);
		return 1;
	}

	*_joinrules = joinrules;
	return 0;
}

static int get_ev_member(const json_object *obj, ev_member_t **_member)
{
	ev_member_t *member = malloc(sizeof(*member));
	if (!member)
		return 1;
	memset(member, 0, sizeof(*member));
	member->isdirect = -1;
	list_init(&member->invite_room_events);

	if (json_get_object_as_string_(obj, "avatar_url", &member->avatarurl) == -1)
		goto err_free_member;

	if (json_get_object_as_string_(obj, "displayname", &member->displayname) == -1)
		goto err_free_member;

	if (json_get_object_as_enum_(obj, "membership", (int *)&member->membership,
				MEMBERSHIP_NUM, membership_strs))
		goto err_free_member;

	json_get_object_as_bool_(obj, "is_direct", &member->isdirect);

	json_object *tpinvite;
	if (json_object_object_get_ex(obj, "third_party_invite", &tpinvite)) {
		if (json_get_object_as_string_(tpinvite, "display_name", &member->displayname))
			goto err_free_member;

		json_object *_signed;
		if (!json_object_object_get_ex(tpinvite, "signed", &_signed))
			goto err_free_member;

		thirdparty_invite_t tpinvite;
		if (json_get_object_as_string_(_signed, "mxid", &tpinvite.mxid))
			goto err_free_member;

		if (json_get_object_as_object_(_signed, "signatures", &tpinvite.signatures))
			goto err_free_member;

		if (json_get_object_as_string_(_signed, "token", &tpinvite.token))
			goto err_free_member;
	}

	json_object *usigned;
	if (json_object_object_get_ex(obj, "unsigned", &usigned)) {
		json_object *state;
		if (json_object_object_get_ex(obj, "invite_room_state", &state)) {
			if (get_event_list(state, &member->invite_room_events))
				goto err_free_member;
		}
	}

	*_member = member;
	return 0;

err_free_member:
	free_ev_member(member);
	return 1;
}

static int get_ev_powerlevels(const json_object *obj, ev_powerlevels_t **_powerlevels)
{
	ev_powerlevels_t *powerlevels = malloc(sizeof(*powerlevels));
	if (!powerlevels)
		return 1;
	memset(powerlevels, 0, sizeof(*powerlevels));
	powerlevels->ban = -1;
	powerlevels->invite = -1;
	powerlevels->kick = -1;
	powerlevels->redact = -1;
	powerlevels->statedefault = -1;

	list_init(&powerlevels->events);
	powerlevels->eventdefault = -1;

	list_init(&powerlevels->users);
	powerlevels->usersdefault = -1;

	powerlevels->roomnotif = -1;

	json_get_object_as_int_(obj, "ban", &powerlevels->ban);
	json_get_object_as_int_(obj, "invite", &powerlevels->invite);
	json_get_object_as_int_(obj, "kick", &powerlevels->kick);
	json_get_object_as_int_(obj, "redact", &powerlevels->redact);
	json_get_object_as_int_(obj, "state_default", &powerlevels->statedefault);

	json_object *events;
	if (json_object_object_get_ex(obj, "events", &events)) {
		json_object_object_foreach(events, k, v) {
			event_powerlevel_t *plevel = malloc(sizeof(*plevel));
			if (!plevel)
				goto err_free_powerlevels;
			
			plevel->type = str2enum(k, eventtype_strs, EVENT_NUM);
			plevel->level = json_object_get_int(v);

			list_add(&powerlevels->events, &plevel->entry);
		}
	}
	json_get_object_as_int_(obj, "events_default", &powerlevels->eventdefault);

	json_object *users;
	if (json_object_object_get_ex(obj, "users", &users)) {
		json_object_object_foreach(users, k, v) {
			user_powerlevel_t *plevel = malloc(sizeof(*plevel));
			if (!plevel)
				goto err_free_powerlevels;

			char *id = strdup(k);
			if (!id) {
				free(plevel);
				goto err_free_powerlevels;
			}
			plevel->id = id;
			plevel->level = json_object_get_int(v);

			list_add(&powerlevels->events, &plevel->entry);
		}
	}
	json_get_object_as_int_(obj, "users_default", &powerlevels->usersdefault);

	json_object *notif;
	if (json_object_object_get_ex(obj, "notifications", &notif)) {
		json_get_object_as_int_(notif, "room", &powerlevels->roomnotif);
	}

	*_powerlevels = powerlevels;
	return 0;

err_free_powerlevels:
	free_ev_powerlevels(powerlevels);
	return 1;
}

static int get_ev_redaction(const json_object *obj, ev_redaction_t **_redaction)
{
	ev_redaction_t *redaction = malloc(sizeof(*redaction));
	if (!redaction)
		return 1;
	memset(redaction, 0, sizeof(*redaction));

	if (json_get_object_as_string_(obj, "reason", &redaction->reason) == -1) {
		free(redaction);
		return 1;
	}

	*_redaction = redaction;
	return 0;
}

static int get_ev_name(const json_object *obj, ev_name_t **_name)
{
	ev_name_t *name = malloc(sizeof(*name));
	if (!name)
		return 1;
	memset(name, 0, sizeof(*name));

	if (json_get_object_as_string_(obj, "name", &name->name)) {
		free(name);
		return 1;
	}

	*_name = name;
	return 0;
}

static int get_ev_topic(const json_object *obj, ev_topic_t **_topic)
{
	ev_topic_t *topic = malloc(sizeof(*topic));
	if (!topic)
		return 1;
	memset(topic, 0, sizeof(*topic));

	if (json_get_object_as_string_(obj, "topic", &topic->topic)) {
		free(topic);
		return 1;
	}

	*_topic = topic;
	return 0;
}

static int get_avatar_image_info(const json_object *obj, avatar_image_info_t *info)
{
	json_get_object_as_uint64_(obj, "h", &info->h);
	json_get_object_as_uint64_(obj, "w", &info->w);

	if (json_get_object_as_string_(obj, "mimetype", &info->mimetype))
		return 1;

	json_get_object_as_uint64_(obj, "size", &info->size);

	return 0;
}
static int get_ev_avatar(const json_object *obj, ev_avatar_t **_avatar)
{
	ev_avatar_t *avatar = malloc(sizeof(*avatar));
	if (!avatar)
		return 1;
	memset(avatar, 0, sizeof(*avatar));

	if (json_get_object_as_string_(obj, "url", &avatar->url))
		goto err_free_avatar;

	json_object *info;
	if (json_object_object_get_ex(obj, "info", &info)) {
		if (get_avatar_image_info(info, &avatar->info))
			goto err_free_avatar;

		if (json_get_object_as_string_(info, "thumbnail_url", &avatar->thumburl) == -1)
			goto err_free_avatar;

		// TODO: Get thumbnail file

		json_object *thumbinfo;
		if (json_object_object_get_ex(obj, "thumbnail_info", &thumbinfo)
				&& get_avatar_image_info(thumbinfo, &avatar->thumbinfo)) {
			goto err_free_avatar;
		}
	}

	*_avatar = avatar;
	return 0;

err_free_avatar:
	free_ev_avatar(avatar);
	return 1;
}

static int get_ev_encryption(const json_object *obj, ev_encryption_t **_encryption)
{
	ev_encryption_t *encryption = malloc(sizeof(*encryption));
	if (!encryption)
		return 1;
	memset(encryption, 0, sizeof(*encryption));
	encryption->rotmsgnum = 604800000;
	encryption->rotperiod = 100;

	if (json_get_object_as_string_(obj, "algorithm", &encryption->algorithm))
		goto err_free_encryption;

	json_get_object_as_uint64_(obj, "rotation_period_ms", &encryption->rotperiod);
	json_get_object_as_uint64_(obj, "rotation_period_ms", &encryption->rotmsgnum);

	*_encryption = encryption;
	return 0;

err_free_encryption:
	free_ev_encryption(encryption);
	return 1;
}

static int get_ev_history_visibility(const json_object *obj, ev_history_visibility_t **_visib)
{
	ev_history_visibility_t *visib = malloc(sizeof(*visib));
	if (!visib)
		return 1;
	memset(visib, 0, sizeof(*visib));

	if (json_get_object_as_enum_(obj, "history_visibility", (int *)&visib->visib,
				HISTVISIB_NUM, history_visibility_strs))
		return 1;

	*_visib = visib;
	return 0;

err_free_visib:
	free_ev_history_visibility(visib);
	return 1;
}

static int get_message_text(const json_object *obj, message_text_t **_msg)
{
	message_text_t *msg = malloc(sizeof(*msg));
	if (!msg)
		return 1;
	memset(msg, 0, sizeof(*msg));

	if (json_get_object_as_string_(obj, "format", &msg->fmt) == -1)
		goto err_free_msg;

	if (json_get_object_as_string_(obj, "formatted_body", &msg->fmtbody) == -1)
		goto err_free_msg;

	*_msg = msg;
	return 0;

err_free_msg:
	free_message_text(msg);
	return 1;
}
static int get_message_emote(const json_object *obj, message_emote_t **_msg)
{
	message_emote_t *msg = malloc(sizeof(*msg));
	if (!msg)
		return 1;
	memset(msg, 0, sizeof(*msg));

	if (json_get_object_as_string_(obj, "format", &msg->fmt) == -1)
		goto err_free_msg;

	if (json_get_object_as_string_(obj, "formatted_body", &msg->fmtbody) == -1)
		goto err_free_msg;

	*_msg = msg;
	return 0;

err_free_msg:
	free_message_emote(msg);
	return 1;
}
static int get_message_content(const json_object *obj, msg_type_t type, void **content)
{
	int err;
	switch (type) {
	case MSG_TEXT:
		err = get_message_text(obj, (message_text_t **)content);
		break;
	case MSG_EMOTE:
		err = get_message_emote(obj, (message_emote_t **)content);
		break;
	default:
		assert(0);
	}
	if (err)
		return 1;

	return 0;
}
static int get_ev_message(const json_object *obj, ev_message_t **_msg)
{
	ev_message_t *msg = malloc(sizeof(*msg));
	if (!msg)
		return 1;
	memset(msg, 0, sizeof(*msg));

	json_get_object_as_enum_(obj, "msgtype", (int *)&msg->type, MSG_NUM, msg_type_strs);

	if (json_get_object_as_string_(obj, "body", &msg->body))
		return 1;

	if (get_message_content(obj, msg->type, &msg->content))
		goto err_free_msg;

	return 0;

err_free_msg:
	free_ev_message(msg);
	return 1;
}

static int get_event_content(const json_object *obj, eventtype_t type, void **content)
{
	int err;
	switch (type) {
	case EVENT_CANONALIAS:
		err = get_ev_canonalias(obj, (ev_canonalias_t **)content);
		break;
	case EVENT_CREATE:
		err = get_ev_create(obj, (ev_create_t **)content);
		break;
	case EVENT_JOINRULES:
		err = get_ev_joinrules(obj, (ev_joinrules_t **)content);
		break;
	case EVENT_MEMBER:
		err = get_ev_member(obj, (ev_member_t **)content);
		break;
	case EVENT_POWERLEVELS:
		err = get_ev_powerlevels(obj, (ev_powerlevels_t **)content);
		break;
	case EVENT_NAME:
		err = get_ev_name(obj, (ev_name_t **)content);
		break;
	case EVENT_TOPIC:
		err = get_ev_topic(obj, (ev_topic_t **)content);
		break;
	case EVENT_AVATAR:
		err = get_ev_avatar(obj, (ev_avatar_t **)content);
		break;
	case EVENT_ENCRYPTION:
		err = get_ev_encryption(obj, (ev_encryption_t **)content);
		break;
	case EVENT_HISTORY_VISIBILITY:
		err = get_ev_history_visibility(obj, (ev_history_visibility_t **)content);
		break;
	case EVENT_REDACTION:
		err = get_ev_redaction(obj, (ev_redaction_t **)content);
		break;
	case EVENT_MESSAGE:
		err = get_ev_message(obj, (ev_message_t **)content);
		break;
	default:
		assert(0);
	}
	if (err)
		return 1;

	return 0;
}
static int get_roomevent_fields(const json_object *obj, event_t *event)
{
	if (json_get_object_as_string_(obj, "event_id", &event->id))
		return 1;

	if (json_get_object_as_string_(obj, "sender", &event->sender))
		return 1;

	json_get_object_as_uint64_(obj, "origin_server_ts", &event->ts);

	json_object *usigned;
	if (json_object_object_get_ex(obj, "unsigned", &usigned)) {
		json_get_object_as_uint64_(usigned, "age", &event->age);

		json_object *redactreason;
		if (json_object_object_get_ex(usigned, "redacted_because", &redactreason)
				&& get_event(redactreason, &event->redactreason))
			return 1;

		if (json_get_object_as_string_(usigned, "transaction_id", &event->txnid) == -1)
			return 1;
	}

	return 0;
}
static int get_statevent_fields(const json_object *obj, event_t *event)
{
	if (get_roomevent_fields(obj, event))
		return 1;

	if (json_get_object_as_string_(obj, "state_key", &event->statekey))
		return 1;

	json_object *prevcontent;
	if (json_object_object_get_ex(obj, "prev_content", &prevcontent)) {
		if (get_event_content(obj, event->type, &event->prevcontent))
			return 1;
	}

	return 0;
}
static int get_event(const json_object *obj, event_t **_event)
{
	event_t *event = malloc(sizeof(*event));
	if (!event)
		return 1;
	memset(event, 0, sizeof(*event));
	
	json_get_object_as_enum_(obj, "type", (int *)&event->type, EVENT_NUM, eventtype_strs);

	if (is_roomevent(event->type) && get_roomevent_fields(obj, event))
		goto err_free_event;

	if (is_statevent(event->type) && get_statevent_fields(obj, event))
		goto err_free_event;

	if (event->type == EVENT_REDACTION &&
			json_get_object_as_string_(obj, "redacts", &event->redacts)) {
		goto err_free_event;
	}

	json_object *content;
	if (!json_object_object_get_ex(obj, "content", &content))
		goto err_free_event;

	if (get_event_content(content, event->type, &event->content))
		goto err_free_event;

	*_event = event;
	return 0;

err_free_event:
	free_event(event);
	return 1;
}

static int get_event_list(const json_object *obj, listentry_t *events)
{
	size_t n = json_object_array_length(obj);
	for (size_t i = 0; i < n; ++i) {
		json_object *o = json_object_array_get_idx(obj, i);

		event_t *event;
		if (get_event(o, &event))
			goto err_free_eventlist;

		list_add(events, &event->entry);
	}

	return 0;

err_free_eventlist:
	list_free(events, event_t, entry, free_event);
	return 1;
}

static void redact_member_content(ev_member_t *member)
{
	free(member->avatarurl);
	member->avatarurl = NULL;

	free(member->displayname);
	member->displayname = NULL;

	member->isdirect = -1;

	free(member->thirdparty_invite.displayname);
	member->thirdparty_invite.displayname = NULL;

	free(member->thirdparty_invite.mxid);
	member->thirdparty_invite.mxid = NULL;

	free(member->thirdparty_invite.token);
	member->thirdparty_invite.token = NULL;

	json_object_put(member->thirdparty_invite.signatures);
	member->thirdparty_invite.signatures = NULL;

	list_free(&member->invite_room_events, event_t, entry, free_event);
	list_init(&member->invite_room_events);
}
static void redact_create_content(ev_create_t *create)
{
	create->federate = -1;

	free(create->version);
	create->version = NULL;

	free(create->previd);
	create->previd = NULL;

	free(create->prev_last_eventid);
	create->prev_last_eventid = NULL;
}
static void redact_powerlevels_content(ev_powerlevels_t *powerlevels)
{
	powerlevels->invite = -1;
	powerlevels->roomnotif = -1;
}
static void redact(event_t *event)
{
	switch (event->type) {
		case EVENT_MEMBER:;
			redact_member_content(event->content);
			break;
		case EVENT_CREATE:
			redact_create_content(event->content);
			break;
		case EVENT_JOINRULES:
			break;
		case EVENT_POWERLEVELS:
			redact_powerlevels_content(event->content);
			break;
		case EVENT_HISTORY_VISIBILITY:
			break;
		default:
			free_event_content(event->type, event->content);
			event->content = NULL;
			break;
	}

	event->age = 0;

	free(event->redactreason);
	event->redactreason = NULL;

	free(event->txnid);
	event->txnid = NULL;

	/* probably not necessary */
	free(event->redacts);
	event->redacts = NULL;

	free_event_content(event->type, event->prevcontent);
	event->prevcontent = NULL;
}

static int get_room_summary(const json_object *obj, room_summary_t *summary)
{
	if (json_get_object_as_string_array_(obj, "m.heroes", &summary->heroes) == -1)
		return 1;

	json_get_object_as_int_(obj, "m.joined_member_count", &summary->njoined);
	json_get_object_as_int_(obj, "m.invited_member_count", &summary->ninvited);

	return 0;
}
static int update_timeline_chunks(const json_object *obj,
		listentry_t *chunks, event_chunk_type_t chunktype)
{
	event_chunk_t *chunk = NULL;
	if (!list_empty(chunks))
		chunk = list_entry_content(chunks->prev, event_chunk_t, entry);

	if (!chunk || chunk->type != chunktype) {
		event_chunk_t *c = malloc(sizeof(*c));
		if (!c)
			return 1;
		c->type = EVENT_CHUNK_MESSAGE;
		list_init(&c->events);
		list_add(chunks, &c->entry);

		chunk = c;
	}

	size_t n = json_object_array_length(obj);
	for (size_t i = 0; i < n; ++i) {
		json_object *o = json_object_array_get_idx(obj, i);

		event_t *event;
		if (get_event(o, &event))
			return 1;

		if (event->type == EVENT_REDACTION) {
			event_t *ev = find_event(chunks, event->redacts);
			assert(ev);

			redact(ev);
		}

		list_add(&chunk->events, &event->entry);
	}

	return 0;
}
static int update_joined_room_history(const json_object *obj, room_history_t *history)
{
	json_object *summary;
	if (json_object_object_get_ex(obj, "summary", &summary)
			&& get_room_summary(summary, &history->summary)) {
		return 1;
	}

	json_object *events;
	json_object *state;
	if (json_object_object_get_ex(obj, "state", &state)
			&& json_object_object_get_ex(obj, "events", &events)
			&& update_timeline_chunks(events,
				&history->timeline.chunks, EVENT_CHUNK_STATE)) {
		return 1;
	}

	json_object *timeline;
	if (json_object_object_get_ex(obj, "timeline", &timeline)) {
		json_get_object_as_int_(timeline, "limited", &history->timeline.limited);

		if (json_get_object_as_string_(timeline, "prev_batch",
					&history->timeline.prevbatch) == -1) {
			return 1;
		}

		if (json_object_object_get_ex(timeline, "events", &events)
				&& update_timeline_chunks(events,
					&history->timeline.chunks, EVENT_CHUNK_MESSAGE)) {
			return 1;
		}
	}

	// TODO: ephemeral and account events

	json_object *unreadnotif;
	if (json_object_object_get_ex(obj, "unread_notifications", &unreadnotif)) {
		json_get_object_as_int_(unreadnotif, "highlight_count",
				&history->notif_highlight_count);
		json_get_object_as_int_(unreadnotif, "notification_count",
				&history->notif_count);
	}

	return 0;
}
static int update_invited_room_history(const json_object *obj, room_history_t *history)
{
	json_object *events;
	json_object *state;
	if (json_object_object_get_ex(obj, "invite_state", &state)
			&& json_object_object_get_ex(obj, "events", &events)
			&& update_timeline_chunks(events,
				&history->timeline.chunks, EVENT_CHUNK_STATE)) {
		return 1;
	}

	return 0;
}
static int update_left_room_history(const json_object *obj, room_history_t *history)
{
	json_object *events;
	json_object *state;
	if (json_object_object_get_ex(obj, "state", &state)
			&& json_object_object_get_ex(obj, "events", &events)
			&& update_timeline_chunks(events,
				&history->timeline.chunks, EVENT_CHUNK_STATE)) {
		return 1;
	}

	json_object *timeline;
	if (json_object_object_get_ex(obj, "timeline", &timeline)) {
		json_get_object_as_int_(timeline, "limited", &history->timeline.limited);

		if (json_get_object_as_string_(timeline, "prev_batch",
					&history->timeline.prevbatch)) {
			return 1;
		}

		if (json_object_object_get_ex(timeline, "events", &events)
				&& update_timeline_chunks(events,
					&history->timeline.chunks, EVENT_CHUNK_MESSAGE)) {
			return 1;
		}
	}

	// TODO: account data

	return 0;
}
int update_room_histories(json_object *obj, listentry_t *joined,
		listentry_t *invited, listentry_t *left)
{
	json_object *join;
	if (json_object_object_get_ex(obj, "join", &join)) {
		json_object_object_foreach(join, k, v) {
			_room_t *r = find_room(joined, k);
			if (!r) {
				r = new_room(k, ROOM_CONTEXT_JOIN);
				if (!r)
					return 1;

				list_add(joined, &r->entry);
			}

			if (update_joined_room_history(v, r->history))
				return 1;

			r->dirty = 1;
		}
	}

	json_object *invite;
	if (json_object_object_get_ex(obj, "invite", &invite)) {
		json_object_object_foreach(invite, k, v) {
			_room_t *r = find_room(invited, k);
			if (!r) {
				r = new_room(k, ROOM_CONTEXT_INVITE);
				if (!r)
					return 1;

				list_add(invited, &r->entry);
			}

			if (update_invited_room_history(v, r->history))
				return 1;

			r->dirty = 1;
		}
	}

	json_object *leave;
	if (json_object_object_get_ex(obj, "leave", &leave)) {
		json_object_object_foreach(leave, k, v) {
			_room_t *r = find_room(left, k);
			if (!r) {
				r = new_room(k, ROOM_CONTEXT_LEAVE);
				if (!r)
					return 1;

				list_add(left, &r->entry);
			}

			if (update_left_room_history(v, r->history))
				return 1;

			r->dirty = 1;
		}
	}

	return 0;
}

int get_presence(const json_object *obj, listentry_t *events)
{
	return 0;
}
int get_account_data(const json_object *obj, listentry_t *events)
{
	return 0;
}
// TODO: implement get_to_device()
