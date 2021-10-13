#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // debug

#include <json-c/json.h>

#include "lib/hjson.h"
#include "mtx/state/parse.h"

static int get_event(const json_object *obj, mtx_event_t **event);
static int get_event_list(const json_object *obj, mtx_listentry_t *events);

static int get_ev_canonalias(const json_object *obj, mtx_ev_canonalias_t **_canonalias)
{
	mtx_ev_canonalias_t *canonalias = malloc(sizeof(*canonalias));
	if (!canonalias)
		return 1;
	memset(canonalias, 0, sizeof(*canonalias));

	if (json_rpl_string_(obj, "alias", &canonalias->alias) == -1)
		goto err_free_canonalias;

	if (json_rpl_string_array(obj, "alt_aliases", &canonalias->altaliases) == -1)
		goto err_free_canonalias;

	*_canonalias = canonalias;
	return 0;

err_free_canonalias:
	free_ev_canonalias(canonalias);
	return 1;
}

static int get_ev_create(const json_object *obj, mtx_ev_create_t **_create)
{
	mtx_ev_create_t *create = malloc(sizeof(*create));
	if (!create)
		return 1;
	memset(create, 0, sizeof(*create));
	create->federate = 1;

	char *creator = strdup("1");
	if (!creator)
		goto err_free_create;
	create->creator = creator;

	if (json_rpl_string_(obj, "creator", &create->creator))
		goto err_free_create;

	json_get_bool_(obj, "m.federate", &create->federate);

	if (json_rpl_string_(obj, "room_version", &create->version) == -1)
		goto err_free_create;

	json_object *pred;
	if (json_object_object_get_ex(obj, "predecessor", &pred)) {
		if (json_rpl_string_(pred, "room_id", &create->previd))
			goto err_free_create;

		if (json_rpl_string_(pred, "event_id", &create->prev_last_eventid))
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

static int get_ev_joinrules(const json_object *obj, mtx_ev_joinrules_t **_joinrules)
{
	mtx_ev_joinrules_t *joinrules = malloc(sizeof(*joinrules));
	if (!joinrules)
		return 1;

	if (json_get_enum_(obj, "join_rule", (int *)&joinrules->rule,
				JOINRULE_NUM, joinrule_strs)) {
		free(joinrules);
		return 1;
	}

	*_joinrules = joinrules;
	return 0;
}

static int get_ev_member(const json_object *obj, mtx_ev_member_t **_member)
{
	mtx_ev_member_t *member = malloc(sizeof(*member));
	if (!member)
		return 1;
	memset(member, 0, sizeof(*member));
	member->isdirect = -1;
	mtx_list_init(&member->invite_room_events);

	if (json_rpl_string_(obj, "avatar_url", &member->avatarurl) == -1)
		goto err_free_member;

	if (json_rpl_string_(obj, "displayname", &member->displayname) == -1)
		goto err_free_member;

	if (json_get_enum_(obj, "membership", (int *)&member->membership,
				MEMBERSHIP_NUM, mtx_membership_strs))
		goto err_free_member;

	json_get_bool_(obj, "is_direct", &member->isdirect);

	json_object *tpinvite;
	if (json_object_object_get_ex(obj, "third_party_invite", &tpinvite)) {
		if (json_rpl_string_(tpinvite, "display_name", &member->displayname))
			goto err_free_member;

		json_object *_signed;
		if (!json_object_object_get_ex(tpinvite, "signed", &_signed))
			goto err_free_member;

		mtx_thirdparty_invite_t tpinvite;
		if (json_rpl_string_(_signed, "mxid", &tpinvite.mxid))
			goto err_free_member;

		if (json_dup_object_(_signed, "signatures", &tpinvite.signatures))
			goto err_free_member;

		if (json_rpl_string_(_signed, "token", &tpinvite.token))
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

static int get_ev_powerlevels(const json_object *obj, mtx_ev_powerlevels_t **_powerlevels)
{
	mtx_ev_powerlevels_t *powerlevels = malloc(sizeof(*powerlevels));
	if (!powerlevels)
		return 1;
	memset(powerlevels, 0, sizeof(*powerlevels));
	powerlevels->ban = -1;
	powerlevels->invite = -1;
	powerlevels->kick = -1;
	powerlevels->redact = -1;
	powerlevels->statedefault = -1;

	mtx_list_init(&powerlevels->events);
	powerlevels->eventdefault = -1;

	mtx_list_init(&powerlevels->users);
	powerlevels->usersdefault = -1;

	powerlevels->roomnotif = -1;

	json_get_int_(obj, "ban", &powerlevels->ban);
	json_get_int_(obj, "invite", &powerlevels->invite);
	json_get_int_(obj, "kick", &powerlevels->kick);
	json_get_int_(obj, "redact", &powerlevels->redact);
	json_get_int_(obj, "state_default", &powerlevels->statedefault);

	json_object *events;
	if (json_object_object_get_ex(obj, "events", &events)) {
		json_object_object_foreach(events, k, v) {
			mtx_event_powerlevel_t *plevel = malloc(sizeof(*plevel));
			if (!plevel)
				goto err_free_powerlevels;
			
			plevel->type = str2enum(k, eventtype_strs, EVENT_NUM);
			plevel->level = json_object_get_int(v);

			mtx_list_add(&powerlevels->events, &plevel->entry);
		}
	}
	json_get_int_(obj, "events_default", &powerlevels->eventdefault);

	json_object *users;
	if (json_object_object_get_ex(obj, "users", &users)) {
		json_object_object_foreach(users, k, v) {
			mtx_user_powerlevel_t *plevel = malloc(sizeof(*plevel));
			if (!plevel)
				goto err_free_powerlevels;

			char *id = strdup(k);
			if (!id) {
				free(plevel);
				goto err_free_powerlevels;
			}
			plevel->id = id;
			plevel->level = json_object_get_int(v);

			mtx_list_add(&powerlevels->users, &plevel->entry);
		}
	}
	json_get_int_(obj, "users_default", &powerlevels->usersdefault);

	json_object *notif;
	if (json_object_object_get_ex(obj, "notifications", &notif)) {
		json_get_int_(notif, "room", &powerlevels->roomnotif);
	}

	*_powerlevels = powerlevels;
	return 0;

err_free_powerlevels:
	free_ev_powerlevels(powerlevels);
	return 1;
}

static int get_ev_redaction(const json_object *obj, mtx_ev_redaction_t **_redaction)
{
	mtx_ev_redaction_t *redaction = malloc(sizeof(*redaction));
	if (!redaction)
		return 1;
	memset(redaction, 0, sizeof(*redaction));

	if (json_rpl_string_(obj, "reason", &redaction->reason) == -1) {
		free(redaction);
		return 1;
	}

	*_redaction = redaction;
	return 0;
}

static int get_ev_name(const json_object *obj, mtx_ev_name_t **_name)
{
	mtx_ev_name_t *name = malloc(sizeof(*name));
	if (!name)
		return 1;
	memset(name, 0, sizeof(*name));

	if (json_rpl_string_(obj, "name", &name->name)) {
		free(name);
		return 1;
	}

	*_name = name;
	return 0;
}

static int get_ev_topic(const json_object *obj, mtx_ev_topic_t **_topic)
{
	mtx_ev_topic_t *topic = malloc(sizeof(*topic));
	if (!topic)
		return 1;
	memset(topic, 0, sizeof(*topic));

	if (json_rpl_string_(obj, "topic", &topic->topic)) {
		free(topic);
		return 1;
	}

	*_topic = topic;
	return 0;
}

static int get_avatar_image_info(const json_object *obj, mtx_avatar_image_info_t *info)
{
	json_get_uint64_(obj, "h", &info->h);
	json_get_uint64_(obj, "w", &info->w);

	if (json_rpl_string_(obj, "mimetype", &info->mimetype))
		return 1;

	json_get_uint64_(obj, "size", &info->size);

	return 0;
}
static int get_ev_avatar(const json_object *obj, mtx_ev_avatar_t **_avatar)
{
	mtx_ev_avatar_t *avatar = malloc(sizeof(*avatar));
	if (!avatar)
		return 1;
	memset(avatar, 0, sizeof(*avatar));

	if (json_rpl_string_(obj, "url", &avatar->url))
		goto err_free_avatar;

	json_object *info;
	if (json_object_object_get_ex(obj, "info", &info)) {
		if (get_avatar_image_info(info, &avatar->info))
			goto err_free_avatar;

		if (json_rpl_string_(info, "thumbnail_url", &avatar->thumburl) == -1)
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

static int get_ev_encryption(const json_object *obj, mtx_ev_encryption_t **_encryption)
{
	mtx_ev_encryption_t *encryption = malloc(sizeof(*encryption));
	if (!encryption)
		return 1;
	memset(encryption, 0, sizeof(*encryption));
	encryption->rotmsgnum = 604800000;
	encryption->rotperiod = 100;

	if (json_rpl_string_(obj, "algorithm", &encryption->algorithm))
		goto err_free_encryption;

	json_get_uint64_(obj, "rotation_period_ms", &encryption->rotperiod);
	json_get_uint64_(obj, "rotation_period_ms", &encryption->rotmsgnum);

	*_encryption = encryption;
	return 0;

err_free_encryption:
	free_ev_encryption(encryption);
	return 1;
}

static int get_ciphertext_info(const json_object *obj,
		const char *identkey, mtx_ciphertext_info_t **_info)
{
	mtx_ciphertext_info_t *info = malloc(sizeof(*info));
	if (!info)
		return 1;
	memset(info, 0, sizeof(*info));

	if (strrpl(&info->identkey, identkey))
		goto err_free_info;

	if (json_rpl_string_(obj, "body", &info->body) == -1) {
		goto err_free_info;
	}

	json_get_int_(obj, "type", &info->type);

	*_info = info;
	return 0;

err_free_info:
	free_ciphertext_info(info);
	return 1;
}
static int get_ev_encrypted(const json_object *obj, mtx_ev_encrypted_t **_encrypted)
{
	mtx_ev_encrypted_t *encrypted = malloc(sizeof(*encrypted));
	if (!encrypted)
		return 1;
	memset(encrypted, 0, sizeof(*encrypted));

	if (json_rpl_string_(obj, "algorithm", &encrypted->algorithm))
		goto err_free_encrypted;

	const char *algolm = mtx_crypt_algorithm_strs[MTX_CRYPT_ALGORITHM_OLM];
	const char *algmegolm = mtx_crypt_algorithm_strs[MTX_CRYPT_ALGORITHM_MEGOLM];
	if (strcmp(encrypted->algorithm, algolm) == 0) {
		mtx_list_init(&encrypted->olm.ciphertext);

		json_object *ciphertext;
		if (json_object_object_get_ex(obj, "ciphertext", &ciphertext))
			goto err_free_encrypted;

		json_object_object_foreach(ciphertext, k, v) {
			mtx_ciphertext_info_t *info;
			if (get_ciphertext_info(v, k, &info))
				goto err_free_encrypted;

			mtx_list_add(&encrypted->olm.ciphertext, &info->entry);
		}
	} else if (strcmp(encrypted->algorithm, algmegolm) == 0) {
		if (json_rpl_string_(obj, "ciphertext", &encrypted->megolm.ciphertext))
			goto err_free_encrypted;

		if (json_rpl_string_(obj, "device_id", &encrypted->megolm.deviceid))
			goto err_free_encrypted;

		if (json_rpl_string_(obj, "session_id", &encrypted->megolm.sessionid))
			goto err_free_encrypted;
	} else {
		assert(0);
	}

	*_encrypted = encrypted;
	return 0;

err_free_encrypted:
	free_ev_encrypted(encrypted);
	return 1;
}

static int get_ev_room_key_request(const json_object *obj, mtx_ev_room_key_request_t **_request)
{
	mtx_ev_room_key_request_t *request = malloc(sizeof(*request));
	if (!request)
		return 1;
	memset(request, 0, sizeof(*request));

	if (json_get_enum_(obj, "action", (int *)&request->action,
				MTX_KEY_REQUEST_NUM, mtx_key_request_action_strs)) {
		goto err_free_request;
	}
	int cancellation = request->action == MTX_KEY_REQUEST_CANCEL;
	
	if (!cancellation) {
		if (json_rpl_string_(obj, "algorithm", &request->body.algorithm))
			goto err_free_request;

		if (json_rpl_string_(obj, "room_id", &request->body.roomid))
			goto err_free_request;

		if (json_rpl_string_(obj, "sender_key", &request->body.senderkey))
			goto err_free_request;

		if (json_rpl_string_(obj, "session_id", &request->body.sessionid))
			goto err_free_request;
	}

	if (json_rpl_string_(obj, "requesting_device_id", &request->deviceid))
		goto err_free_request;

	if (json_rpl_string_(obj, "request_id", &request->requestid))
		goto err_free_request;

	*_request = request;
	return 0;

err_free_request:
	free_ev_room_key_request(request);
	return 1;
}

static int get_ev_history_visibility(const json_object *obj, mtx_ev_history_visibility_t **_visib)
{
	mtx_ev_history_visibility_t *visib = malloc(sizeof(*visib));
	if (!visib)
		return 1;
	memset(visib, 0, sizeof(*visib));

	if (json_get_enum_(obj, "history_visibility", (int *)&visib->visib,
				HISTVISIB_NUM, mtx_history_visibility_strs))
		return 1;

	*_visib = visib;
	return 0;

err_free_visib:
	free_ev_history_visibility(visib);
	return 1;
}

static int get_ev_guest_access(const json_object *obj, mtx_ev_guest_access_t **_guestaccess)
{
	mtx_ev_guest_access_t *guestaccess = malloc(sizeof(*guestaccess));
	if (!guestaccess)
		return 1;
	memset(guestaccess, 0, sizeof(*guestaccess));
	
	if (json_get_enum_(obj, "guest_access", (int *)&guestaccess->access,
				MTX_GUEST_ACCESS_NUM, mtx_guest_access_strs))
		return 1;

	*_guestaccess = guestaccess;
	return 0;

err_free_guest_access:
	free_ev_guest_access(guestaccess);
	return 1;
}

static int get_message_text(const json_object *obj, mtx_message_text_t **_msg)
{
	mtx_message_text_t *msg = malloc(sizeof(*msg));
	if (!msg)
		return 1;
	memset(msg, 0, sizeof(*msg));

	if (json_rpl_string_(obj, "format", &msg->fmt) == -1)
		goto err_free_msg;

	if (json_rpl_string_(obj, "formatted_body", &msg->fmtbody) == -1)
		goto err_free_msg;

	*_msg = msg;
	return 0;

err_free_msg:
	free_message_text(msg);
	return 1;
}
static int get_message_emote(const json_object *obj, mtx_message_emote_t **_msg)
{
	mtx_message_emote_t *msg = malloc(sizeof(*msg));
	if (!msg)
		return 1;
	memset(msg, 0, sizeof(*msg));

	if (json_rpl_string_(obj, "format", &msg->fmt) == -1)
		goto err_free_msg;

	if (json_rpl_string_(obj, "formatted_body", &msg->fmtbody) == -1)
		goto err_free_msg;

	*_msg = msg;
	return 0;

err_free_msg:
	free_message_emote(msg);
	return 1;
}
static int get_message_content(const json_object *obj, mtx_msg_type_t type, void **content)
{
	int err;
	switch (type) {
	case MTX_MSG_TEXT:
		err = get_message_text(obj, (mtx_message_text_t **)content);
		break;
	case MTX_MSG_EMOTE:
		err = get_message_emote(obj, (mtx_message_emote_t **)content);
		break;
	default:
		assert(0);
	}
	if (err)
		return 1;

	return 0;
}
static int get_ev_message(const json_object *obj, mtx_ev_message_t **_msg)
{
	mtx_ev_message_t *msg = malloc(sizeof(*msg));
	if (!msg)
		return 1;
	memset(msg, 0, sizeof(*msg));

	json_get_enum_(obj, "msgtype", (int *)&msg->type, MTX_MSG_NUM, mtx_msg_type_strs);

	if (json_rpl_string_(obj, "body", &msg->body))
		return 1;

	if (get_message_content(obj, msg->type, &msg->content))
		goto err_free_msg;

	*_msg = msg;
	return 0;

err_free_msg:
	free_ev_message(msg);
	return 1;
}

static int get_event_content(const json_object *obj, mtx_eventtype_t type, void **content)
{
	int err;
	switch (type) {
	case EVENT_CANONALIAS:
		err = get_ev_canonalias(obj, (mtx_ev_canonalias_t **)content);
		break;
	case EVENT_CREATE:
		err = get_ev_create(obj, (mtx_ev_create_t **)content);
		break;
	case EVENT_JOINRULES:
		err = get_ev_joinrules(obj, (mtx_ev_joinrules_t **)content);
		break;
	case EVENT_MEMBER:
		err = get_ev_member(obj, (mtx_ev_member_t **)content);
		break;
	case EVENT_POWERLEVELS:
		err = get_ev_powerlevels(obj, (mtx_ev_powerlevels_t **)content);
		break;
	case EVENT_NAME:
		err = get_ev_name(obj, (mtx_ev_name_t **)content);
		break;
	case EVENT_TOPIC:
		err = get_ev_topic(obj, (mtx_ev_topic_t **)content);
		break;
	case EVENT_AVATAR:
		err = get_ev_avatar(obj, (mtx_ev_avatar_t **)content);
		break;
	case EVENT_ENCRYPTION:
		err = get_ev_encryption(obj, (mtx_ev_encryption_t **)content);
		break;
	case EVENT_HISTORY_VISIBILITY:
		err = get_ev_history_visibility(obj, (mtx_ev_history_visibility_t **)content);
		break;
	case EVENT_GUEST_ACCESS:
		err = get_ev_guest_access(obj, (mtx_ev_guest_access_t **)content);
		break;
	case EVENT_REDACTION:
		err = get_ev_redaction(obj, (mtx_ev_redaction_t **)content);
		break;
	case EVENT_MESSAGE:
		err = get_ev_message(obj, (mtx_ev_message_t **)content);
		break;
	default:
		assert(0);
	}
	if (err)
		return 1;

	return 0;
}
static int get_roomevent_fields(const json_object *obj, mtx_event_t *event)
{
	if (json_rpl_string_(obj, "event_id", &event->id))
		return 1;

	if (json_rpl_string_(obj, "sender", &event->sender))
		return 1;

	json_get_uint64_(obj, "origin_server_ts", &event->ts);

	json_object *usigned;
	if (json_object_object_get_ex(obj, "unsigned", &usigned)) {
		json_get_uint64_(usigned, "age", &event->age);

		json_object *redactreason;
		if (json_object_object_get_ex(usigned, "redacted_because", &redactreason)
				&& get_event(redactreason, &event->redactreason))
			return 1;

		if (json_rpl_string_(usigned, "transaction_id", &event->txnid) == -1)
			return 1;
	}

	return 0;
}
static int get_statevent_fields(const json_object *obj, mtx_event_t *event)
{
	if (get_roomevent_fields(obj, event))
		return 1;

	if (json_rpl_string_(obj, "state_key", &event->statekey))
		return 1;

	json_object *prevcontent;
	if (json_object_object_get_ex(obj, "prev_content", &prevcontent)) {
		if (get_event_content(obj, event->type, &event->prevcontent))
			return 1;
	}

	return 0;
}
static int get_event(const json_object *obj, mtx_event_t **_event)
{
	mtx_event_t *event = malloc(sizeof(*event));
	if (!event)
		return 1;
	memset(event, 0, sizeof(*event));
	
	json_get_enum_(obj, "type", (int *)&event->type, EVENT_NUM, eventtype_strs);

	if (is_roomevent(event->type) && get_roomevent_fields(obj, event))
		goto err_free_event;

	if (is_statevent(event->type) && get_statevent_fields(obj, event))
		goto err_free_event;

	if (event->type == EVENT_REDACTION &&
			json_rpl_string_(obj, "redacts", &event->redacts)) {
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

static int get_event_list(const json_object *obj, mtx_listentry_t *events)
{
	size_t n = json_object_array_length(obj);
	for (size_t i = 0; i < n; ++i) {
		json_object *o = json_object_array_get_idx(obj, i);

		mtx_event_t *event;
		if (get_event(o, &event))
			goto err_free_eventlist;

		mtx_list_add(events, &event->entry);
	}

	return 0;

err_free_eventlist:
	mtx_list_free(events, mtx_event_t, entry, free_event);
	return 1;
}

static void redact_member_content(mtx_ev_member_t *member)
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

	mtx_list_free(&member->invite_room_events, mtx_event_t, entry, free_event);
	mtx_list_init(&member->invite_room_events);
}
static void redact_create_content(mtx_ev_create_t *create)
{
	create->federate = -1;

	free(create->version);
	create->version = NULL;

	free(create->previd);
	create->previd = NULL;

	free(create->prev_last_eventid);
	create->prev_last_eventid = NULL;
}
static void redact_powerlevels_content(mtx_ev_powerlevels_t *powerlevels)
{
	powerlevels->invite = -1;
	powerlevels->roomnotif = -1;
}
static void redact(mtx_event_t *event)
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

static int get_room_summary(const json_object *obj, mtx_room_summary_t *summary)
{
	if (json_rpl_string_array(obj, "m.heroes", &summary->heroes) == -1)
		return 1;

	json_get_int_(obj, "m.joined_member_count", &summary->njoined);
	json_get_int_(obj, "m.invited_member_count", &summary->ninvited);

	return 0;
}
static int update_timeline_chunks(const json_object *obj,
		mtx_listentry_t *chunks, mtx_event_chunk_type_t chunktype)
{
	size_t n = json_object_array_length(obj);
	if (n == 0)
		return 0;

	mtx_event_chunk_t *chunk = NULL;
	if (!mtx_list_empty(chunks))
		chunk = mtx_list_entry_content(chunks->prev, mtx_event_chunk_t, entry);

	if (!chunk || chunk->type != chunktype) {
		mtx_event_chunk_t *c = malloc(sizeof(*c));
		if (!c)
			return 1;
		c->type = EVENT_CHUNK_MESSAGE;
		mtx_list_init(&c->events);
		mtx_list_add(chunks, &c->entry);

		chunk = c;
	}

	for (size_t i = 0; i < n; ++i) {
		json_object *o = json_object_array_get_idx(obj, i);

		mtx_event_t *event;
		if (get_event(o, &event))
			return 1;

		if (event->type == EVENT_REDACTION) {
			mtx_event_t *ev = find_event(chunks, event->redacts);
			assert(ev);

			redact(ev);
		}

		mtx_list_add(&chunk->events, &event->entry);
	}

	return 0;
}
static int update_joined_room_history(const json_object *obj, mtx_room_history_t *history)
{
	json_object *summary;
	if (json_object_object_get_ex(obj, "summary", &summary)
			&& get_room_summary(summary, &history->summary)) {
		return 1;
	}

	json_object *events;
	json_object *state;
	if (json_object_object_get_ex(obj, "state", &state)
			&& json_object_object_get_ex(state, "events", &events)
			&& update_timeline_chunks(events,
				&history->timeline.chunks, EVENT_CHUNK_STATE)) {
		return 1;
	}

	json_object *timeline;
	if (json_object_object_get_ex(obj, "timeline", &timeline)) {
		json_get_bool_(timeline, "limited", &history->timeline.limited);

		if (json_rpl_string_(timeline, "prev_batch",
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
		json_get_int_(unreadnotif, "highlight_count",
				&history->notif_highlight_count);
		json_get_int_(unreadnotif, "notification_count",
				&history->notif_count);
	}

	return 0;
}
static int update_invited_room_history(const json_object *obj, mtx_room_history_t *history)
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
static int update_left_room_history(const json_object *obj, mtx_room_history_t *history)
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
		json_get_int_(timeline, "limited", &history->timeline.limited);

		if (json_rpl_string_(timeline, "prev_batch",
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
int update_room_histories(json_object *obj, mtx_listentry_t *joined,
		mtx_listentry_t *invited, mtx_listentry_t *left)
{
	json_object *join;
	if (json_object_object_get_ex(obj, "join", &join)) {
		json_object_object_foreach(join, k, v) {
			mtx_room_t *r = find_room(joined, k);
			if (!r) {
				r = new_room(k, MTX_ROOM_CONTEXT_JOIN);
				if (!r)
					return 1;

				mtx_list_add(joined, &r->entry);
			}

			if (update_joined_room_history(v, r->history))
				return 1;

			r->dirty = 1;
		}
	}

	json_object *invite;
	if (json_object_object_get_ex(obj, "invite", &invite)) {
		json_object_object_foreach(invite, k, v) {
			mtx_room_t *r = find_room(invited, k);
			if (!r) {
				r = new_room(k, MTX_ROOM_CONTEXT_INVITE);
				if (!r)
					return 1;

				mtx_list_add(invited, &r->entry);
			}

			if (update_invited_room_history(v, r->history))
				return 1;

			r->dirty = 1;
		}
	}

	json_object *leave;
	if (json_object_object_get_ex(obj, "leave", &leave)) {
		json_object_object_foreach(leave, k, v) {
			mtx_room_t *r = find_room(left, k);
			if (!r) {
				r = new_room(k, MTX_ROOM_CONTEXT_LEAVE);
				if (!r)
					return 1;

				mtx_list_add(left, &r->entry);
			}

			if (update_left_room_history(v, r->history))
				return 1;

			r->dirty = 1;
		}
	}

	return 0;
}

int get_presence(const json_object *obj, mtx_listentry_t *events)
{
	return 0;
}
int get_account_data(const json_object *obj, mtx_listentry_t *events)
{
	return 0;
}
// TODO: implement get_to_device()

int get_to_device(const json_object *obj, mtx_listentry_t *events)
{
	json_object *_events;
	if (json_object_object_get_ex(obj, "events", &_events))
		return 1;

	if (get_event_list(_events, events))
		return 1;

	return 0;
}
