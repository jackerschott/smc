#include <assert.h>
#include <string.h>

#include <json-c/json.h>

#include "lib/array.h"
#include "mtx/state/room.h"

/* history types */
void free_ev_canonalias(ev_canonalias_t *canonalias)
{
	if (!canonalias)
		return;

	free(canonalias->alias);
	for (size_t i = 0; canonalias->altaliases[i] != NULL; ++i) {
		free(canonalias->altaliases[i]);
	}
	free(canonalias);
}

void free_ev_create(ev_create_t *create)
{
	if (!create)
		return;

	free(create->creator);
	free(create->version);
	free(create->previd);
	free(create->prev_last_eventid);
	free(create);
}

void free_ev_joinrules(ev_joinrules_t *joinrules)
{
	if (!joinrules)
		return;

	free(joinrules);
}

void free_ev_member(ev_member_t *member)
{
	if (!member)
		return;

	free(member->avatarurl);
	free(member->displayname);
	list_free(&member->invite_room_events, event_t, entry, free_event);

	thirdparty_invite_t tpinvite = member->thirdparty_invite;
	free(tpinvite.displayname);
	free(tpinvite.mxid);
	json_object_put(tpinvite.signatures);
	free(tpinvite.token);

	free(member);
}

void free_event_powerlevel(event_powerlevel_t *plevel)
{
	if (!plevel)
		return;

	free(plevel);
}
void free_user_powerlevel(user_powerlevel_t *plevel)
{
	if (!plevel)
		return;

	free(plevel->id);
	free(plevel);
}
void free_ev_powerlevels(ev_powerlevels_t *powerlevels)
{
	if (!powerlevels)
		return;

	list_free(&powerlevels->events, event_powerlevel_t, entry, free_event_powerlevel);
	list_free(&powerlevels->users, user_powerlevel_t, entry, free_user_powerlevel);
	free(powerlevels);
}
event_powerlevel_t *find_event_powerlevel(const listentry_t *plevels, eventtype_t type)
{
	for (listentry_t *e = plevels->next; e != plevels; e = e->next) {
		event_powerlevel_t *plevel = list_entry_content(e, event_powerlevel_t, entry);
		if (plevel->type == type)
			return plevel;
	}

	return NULL;
}
user_powerlevel_t *find_user_powerlevel(const listentry_t *plevels, const char *id)
{
	for (listentry_t *e = plevels->next; e != plevels; e = e->next) {
		user_powerlevel_t *plevel = list_entry_content(e, user_powerlevel_t, entry);
		if (strcmp(plevel->id, id) == 0)
			return plevel;
	}

	return NULL;
}

void free_ev_redaction(ev_redaction_t *redaction)
{
	if (!redaction)
		return;

	free(redaction->reason);
	free(redaction);
}

void free_ev_name(ev_name_t *name)
{
	if (!name)
		return;

	free(name->name);
	free(name);
}

void free_ev_avatar(ev_avatar_t *avatar)
{
	if (!avatar)
		return;

	free(avatar->url);
	free(avatar->info.mimetype);

	free(avatar->thumburl);
	free(avatar->thumbinfo.mimetype);
	free(avatar);
}

void free_ev_encryption(ev_encryption_t *encryption)
{
	free(encryption->algorithm);
	free(encryption);
}

void free_ev_history_visibility(ev_history_visibility_t *visib)
{
	free(visib);
}

void free_message_text(message_text_t *msg)
{
	if (!msg)
		return;

	free(msg->fmt);
	free(msg->fmtbody);
	free(msg);
}
void free_message_emote(message_emote_t *msg)
{
	if (!msg)
		return;

	free(msg->fmt);
	free(msg->fmtbody);
	free(msg);
}
void free_message_content(msg_type_t type, void *content)
{
	if (!content)
		return;

	switch (type) {
	case MSG_TEXT:
		free_message_text(content);
		break;
	case MSG_EMOTE:
		free_message_emote(content);
		break;
	default:
		assert(0);
	}
}
void free_ev_message(ev_message_t *msg)
{
	if (!msg)
		return;

	free(msg->body);
	free_message_content(msg->type, msg->content);
	free(msg);
}
message_text_t *duplicate_message_text(message_text_t *msg)
{
	message_text_t *m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(*m));

	if (strrpl(&m->fmt, msg->fmt))
		goto err_free_msg;

	if (strrpl(&m->fmtbody, msg->fmt))
		goto err_free_msg;

	return m;

err_free_msg:
	free_message_text(m);
	return NULL;
}
message_emote_t *duplicate_message_emote(message_emote_t *emote)
{
	message_emote_t *e = malloc(sizeof(*e));
	if (!e)
		return NULL;
	memset(e, 0, sizeof(*e));

	if (strrpl(&e->fmt, emote->fmt))
		goto err_free_msg;

	if (strrpl(&e->fmtbody, emote->fmt))
		goto err_free_msg;

	return e;

err_free_msg:
	free_message_emote(e);
	return 0;
}
void *duplicate_message_content(msg_type_t type, void *content)
{
	void *newcontent;
	switch (type) {
	case MSG_TEXT:
		newcontent = duplicate_message_text(content);
		break;
	case MSG_EMOTE:
		newcontent = duplicate_message_emote(content);
		break;
	default:
		assert(0);
	}
	if (!newcontent)
		return NULL;

	return newcontent;
}

void free_event_content(eventtype_t type, void *content)
{
	switch (type) {
	case EVENT_CANONALIAS:
		free_ev_canonalias(content);
		break;
	case EVENT_CREATE:
		free_ev_create(content);
		break;
	case EVENT_JOINRULES:
		free_ev_joinrules(content);
		break;
	case EVENT_MEMBER:
		free_member(content);
		break;
	case EVENT_POWERLEVELS:
		free_ev_powerlevels(content);
		break;
	case EVENT_NAME:
		free_ev_name(content);
		break;
	case EVENT_AVATAR:
		free_ev_avatar(content);
		break;
	case EVENT_ENCRYPTION:
		free_ev_encryption(content);
		break;
	case EVENT_HISTORY_VISIBILITY:
		free_ev_history_visibility(content);
		break;
	case EVENT_REDACTION:
		free_ev_redaction(content);
		break;
	default:
		assert(0);
	}
}
void free_event(event_t *event)
{
	if (!event)
		return;

	free(event->id);
	free(event->sender);

	free_event(event->redactreason);
	free(event->txnid);

	free(event->statekey);

	free(event->redacts);

	free_event_content(event->type, event->prevcontent);
	free_event_content(event->type, event->content);

	free(event);
}
void free_event_chunk(event_chunk_t *chunk)
{
	if (!chunk)
		return;

	list_free(&chunk->events, event_t, entry, free_event);
}
int is_roomevent(eventtype_t type)
{
	return EVENT_CANONALIAS <= type && type <= EVENT_REDACTION;
}
int is_statevent(eventtype_t type)
{
	return EVENT_CANONALIAS <= type && type <= EVENT_TOMBSTONE;
}
int is_message_event(eventtype_t type)
{
	return EVENT_REDACTION <= type && type <= EVENT_REDACTION;
}
event_t *find_event(listentry_t *chunks, const char *eventid)
{
	for (listentry_t *e = chunks->next; e != chunks; e = e->next) {
		event_chunk_t *chunk = list_entry_content(e, event_chunk_t, entry);
		for (listentry_t *f = chunk->events.next; f != &chunk->events; f = f->next) {
			event_t *ev = list_entry_content(f, event_t, entry);
			if (ev->id != eventid)
				continue;

			return ev;
		}
	}
	return NULL;
}

void free_room_history(room_history_t *history)
{
	if (!history)
		return;

	if (history->summary.heroes) {
		for (size_t i = 0; history->summary.heroes[i]; ++i) {
			free(history->summary.heroes[i]);
		}
	}

	list_free(&history->timeline.chunks, event_chunk_t, entry, free_event_chunk);
	free(history->timeline.prevbatch);

	list_free(&history->ephemeral, event_t, entry, free_event);
	list_free(&history->account, event_t, entry, free_event);
}
void free_room_history_context(_room_t *r)
{
	if (!r)
		return;

	free(r->id);
	free_room_history(r->history);
	free(r);
}
_room_t *find_room(listentry_t *rooms, const char *id)
{
	for (listentry_t *e = rooms->next; e != rooms; e = e->next) {
		_room_t *r = list_entry_content(e, _room_t, entry);

		if (strcmp(r->id, id) == 0)
			return r;
	}
	return NULL;
}

/* direct state types */
void free_member(member_t *m)
{
	if (!m)
		return;

	free(m->userid);
	free(m->displayname);
	free(m->avatarurl);
	free(m);
}
member_t *new_member(void)
{
	member_t *m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(*m));
	m->membership = MEMBERSHIP_NUM;
	m->isdirect = -1;
	
	return m;
}
member_t *find_member(listentry_t *members, const char *userid)
{
	for (listentry_t *e = members->next; e != members; e = e->next) {
		member_t *m = list_entry_content(members, member_t, entry);
		if (strcmp(m->userid, userid) == 0)
			return m;
	}

	return NULL;
}

void free_msg(msg_t *m)
{
	if (!m)
		free(m);

	free(m->body);
	free(m->sender);
	free_message_content(m->type, m->content);
	free(m);
}

void free_room_direct_state_context(_room_t *r)
{
	free(r->creator);
	free(r->version);
	free(r->previd);
	free(r->prev_last_eventid);

	free(r->name);
	free(r->topic);
	free(r->canonalias);
	strarr_free(r->altaliases);

	strarr_free(r->heroes);
	list_free(&r->members, member_t, entry, free_member);

	list_free(&r->powerlevels.events, event_powerlevel_t, entry, free_event_powerlevel);
	list_free(&r->powerlevels.users, user_powerlevel_t, entry, free_user_powerlevel);

	free(r->avatarurl);
	free(r->avatarinfo.mimetype);
	free(r->avatarthumburl);
	free(r->avatarthumbinfo.mimetype);

	list_free(&r->msgs, msg_t, entry, free_msg);

	free(r->crypt.algorithm);
}

/* general */
static room_history_t *new_room_history(void)
{
	room_history_t *history = malloc(sizeof(*history));
	if (!history)
		return NULL;
	memset(history, 0, sizeof(*history));

	list_init(&history->timeline.chunks);
	list_init(&history->ephemeral);
	list_init(&history->account);

	return history;
}
void free_room(_room_t *r)
{
	free_room_direct_state_context(r);
	free_room_history_context(r);
	free(r);
}
_room_t *new_room(const char *id, room_context_t context)
{
	_room_t *r = malloc(sizeof(*r));
	if (!r)
		return NULL;
	memset(r, 0, sizeof(*r));

	r->ninvited = 0;
	r->njoined = 0;
	r->notif_count = 0;
	r->notif_highlight_count = 0;

	list_init(&r->members);

	r->powerlevels.ban = 50;
	r->powerlevels.invite = 50;
	r->powerlevels.kick = 50;
	r->powerlevels.redact = 50;
	r->powerlevels.statedefault = 50;

	list_init(&r->powerlevels.events);
	r->powerlevels.eventdefault = 0;

	list_init(&r->powerlevels.users);
	r->powerlevels.usersdefault = 0;

	r->powerlevels.roomnotif = 50;

	r->joinrule = JOINRULE_NUM;

	r->histvisib = HISTVISIB_NUM;

	list_init(&r->msgs);

	if (strrpl(&r->id, id)) {
		free_room_history_context(r);
		return NULL;
	}

	r->context = context;

	room_history_t *history = new_room_history();
	if (!history) {
		free_room_history_context(r);
		return NULL;
	}
	r->history = history;

	r->dirty = 1;
	return r;
}
void clear_room_direct_state(_room_t *r)
{
	free(r->creator);
	r->creator = NULL;

	free(r->version);
	r->version = NULL;

	r->federate = 1;

	free(r->previd);
	r->previd = NULL;

	free(r->prev_last_eventid);
	r->prev_last_eventid = NULL;

	free(r->name);
	r->name = NULL;
	
	free(r->topic);
	r->topic = NULL;

	free(r->canonalias);
	r->canonalias = NULL;

	strarr_free(r->altaliases);
	r->altaliases = NULL;

	strarr_free(r->heroes);
	r->heroes = NULL;

	r->ninvited = 0;
	r->njoined = 0;
	r->notif_count = 0;
	r->notif_highlight_count = 0;

	list_free(&r->members, member_t, entry, free_member);
	list_init(&r->members);

	r->powerlevels.ban = 50;
	r->powerlevels.invite = 50;
	r->powerlevels.kick = 50;
	r->powerlevels.redact = 50;
	r->powerlevels.statedefault = 50;

	list_free(&r->powerlevels.events, event_powerlevel_t, entry, free_event_powerlevel);
	list_init(&r->powerlevels.events);
	r->powerlevels.eventdefault = 0;

	list_free(&r->powerlevels.users, user_powerlevel_t, entry, free_user_powerlevel);
	list_init(&r->powerlevels.users);
	r->powerlevels.usersdefault = 0;

	r->powerlevels.roomnotif = 50;

	r->joinrule = JOINRULE_NUM;

	r->histvisib = HISTVISIB_NUM;

	free(r->avatarurl);
	r->avatarurl = NULL;

	r->avatarinfo.w = 0;
	r->avatarinfo.h = 0;
	r->avatarinfo.size = 0;
	free(r->avatarinfo.mimetype);
	r->avatarinfo.mimetype = NULL;

	free(r->avatarthumburl);
	r->avatarthumburl = NULL;

	r->avatarthumbinfo.w = 0;
	r->avatarthumbinfo.h = 0;
	r->avatarthumbinfo.size = 0;
	free(r->avatarthumbinfo.mimetype);
	r->avatarthumbinfo.mimetype = NULL;

	list_free(&r->msgs, msg_t, entry, free_msg);
	list_init(&r->msgs);

	r->crypt.enabled = 0;
	free(r->crypt.algorithm);
	r->crypt.algorithm = NULL;
	r->crypt.rotperiod = 0;
	r->crypt.rotmsgnum = 0;

	r->dirty = 1;
}
