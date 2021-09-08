#include <assert.h>
#include <string.h>

#include "lib/array.h"
#include "mtx/state/apply.h"

int apply_event_canonalias(const event_t *event, _room_t *r)
{
	ev_canonalias_t *canonalias = event->content;

	if (canonalias->alias && strrpl(&r->canonalias, canonalias->alias))
		return 1;

	if (canonalias->altaliases && strarr_rpl(&r->altaliases, canonalias->altaliases))
		return 1;

	return 0;
}

int apply_event_create(const event_t *event, _room_t *r)
{
	ev_create_t *create = event->content;

	if (strrpl(&r->creator, create->creator))
		return 1;

	if (strrpl(&r->version, create->version))
		return 1;

	r->federate = create->federate;

	if (create->previd && strrpl(&r->previd, create->previd))
		return 1;

	if (create->prev_last_eventid && strrpl(&r->prev_last_eventid, create->prev_last_eventid))
		return 1;

	return 0;
}

int apply_event_joinrules(const event_t *event, _room_t *r)
{
	ev_joinrules_t *joinrules = event->content;

	r->joinrule = joinrules->rule;

	return 0;
}

int apply_event_member(const event_t *event, _room_t *r)
{
	ev_member_t *member = event->content;

	member_t *m = find_member(&r->members, event->statekey);
	if (!m) {
		m = new_member();
		if (!m)
			return 1;
		list_add(&r->members, &m->entry);
	}

	if (strrpl(&m->userid, event->statekey))
		return 1;

	if (member->avatarurl && strrpl(&m->avatarurl, member->avatarurl))
		return 1;

	if (member->displayname && strrpl(&m->displayname, member->displayname))
		return 1;

	if (member->membership != MEMBERSHIP_NUM)
		m->membership = member->membership;

	if (member->isdirect != -1)
		m->isdirect = member->isdirect;

	// TODO: third party invite

	return 0;
}

int apply_event_powerlevels(const event_t *event, _room_t *r)
{
	ev_powerlevels_t *powerlevels = event->content;

	if (powerlevels->ban != -1)
		r->powerlevels.ban = powerlevels->ban;

	if (powerlevels->invite != -1)
		r->powerlevels.invite = powerlevels->invite;

	if (powerlevels->kick != -1)
		r->powerlevels.kick = powerlevels->kick;

	if (powerlevels->redact != -1)
		r->powerlevels.redact = powerlevels->redact;

	if (powerlevels->statedefault != -1)
		r->powerlevels.statedefault = powerlevels->statedefault;

	for (listentry_t *e = powerlevels->events.next; e != &powerlevels->events; e = e->next) {
		event_powerlevel_t *newplevel = list_entry_content(e, event_powerlevel_t, entry);
		event_powerlevel_t *plevel = find_event_powerlevel(&r->powerlevels.events,
				newplevel->type);
		if (!plevel) {
			plevel = malloc(sizeof(*plevel));
			if (!plevel)
				return 1;
		}
		plevel->type = newplevel->type;
		plevel->level = newplevel->level;
	}

	if (powerlevels->eventdefault != -1)
		r->powerlevels.eventdefault = powerlevels->eventdefault;

	for (listentry_t *e = powerlevels->users.next; e != &powerlevels->users; e = e->next) {
		user_powerlevel_t *newplevel = list_entry_content(e, user_powerlevel_t, entry);
		user_powerlevel_t *plevel = find_user_powerlevel(&r->powerlevels.events,
				newplevel->id);
		if (!plevel) {
			plevel = malloc(sizeof(*plevel));
			if (!plevel)
				return 1;
		}

		if (strrpl(&plevel->id, newplevel->id))
			return 1;
		plevel->level = newplevel->level;
	}

	if (powerlevels->usersdefault != -1)
		r->powerlevels.usersdefault = powerlevels->usersdefault;

	if (powerlevels->roomnotif != -1)
		r->powerlevels.roomnotif = powerlevels->roomnotif;

	return 0;
}

int apply_event_name(const event_t *event, _room_t *r)
{
	ev_name_t *name = event->content;

	if (name->name && strrpl(&r->name, name->name))
		return 1;

	return 0;
}

int apply_event_avatar(const event_t *event, _room_t *r)
{
	ev_avatar_t *avatar = event->content;

	if (avatar->url && strrpl(&r->avatarurl, avatar->url))
		return 1;

	r->avatarinfo.w = avatar->info.w;
	r->avatarinfo.h = avatar->info.h;
	r->avatarinfo.size = avatar->info.size;
	if (strrpl(&r->avatarinfo.mimetype, avatar->info.mimetype))
		return 1;

	if (avatar->thumburl)  {
		if (strrpl(&r->avatarthumburl, avatar->thumburl))
			return 1;

		r->avatarthumbinfo.w = avatar->thumbinfo.w;
		r->avatarthumbinfo.h = avatar->thumbinfo.h;
		r->avatarthumbinfo.size = avatar->thumbinfo.size;
		if (strrpl(&r->avatarthumbinfo.mimetype, avatar->thumbinfo.mimetype))
			return 1;
	}

	return 0;
}

int apply_event_encryption(const event_t *event, _room_t *r)
{
	ev_encryption_t *encryption = event->content;

	r->crypt.enabled = 1;

	if (strrpl(&r->crypt.algorithm, encryption->algorithm))
		return 1;

	r->crypt.rotmsgnum = encryption->rotmsgnum;
	r->crypt.rotperiod = encryption->rotperiod;
	return 0;
}

int apply_event_history_visibility(const event_t *event, _room_t *r)
{
	ev_history_visibility_t *histvisib = event->content;

	r->histvisib = histvisib->visib;
	return 0;
}

int apply_event_message(const event_t *event, _room_t *r)
{
	ev_message_t *msg = event->content;

	msg_t *m = malloc(sizeof(*m));
	if (!m)
		return 1;
	memset(m, 0, sizeof(*m));
	list_add(&r->msgs, &m->entry);

	m->type = msg->type;
	if (strrpl(&m->body, msg->body))
		return 1;

	if (strrpl(&m->sender, event->sender))
		return 1;

	void *content = duplicate_message_content(msg->type, msg->content);
	if (!content)
		return 1;
	m->content = content;

	return 0;
}

int apply_room_summary(_room_t *r, room_summary_t *summary)
{
	if (summary->heroes && strarr_rpl(&r->heroes, summary->heroes))
		return 1;

	if (r->ninvited != -1)
		r->ninvited = summary->ninvited;
	if (r->njoined != -1)
		r->njoined = summary->njoined;

	return 0;
}
int apply_statevent(const event_t *event, _room_t *r)
{
	int err;
	switch (event->type) {
	case EVENT_CANONALIAS:
		err = apply_event_canonalias(event, r);
		break;
	case EVENT_CREATE:
		err = apply_event_create(event, r);
		break;
	case EVENT_JOINRULES:
		err = apply_event_joinrules(event, r);
		break;
	case EVENT_MEMBER:
		err = apply_event_member(event, r);
		break;
	case EVENT_POWERLEVELS:
		err = apply_event_powerlevels(event, r);
		break;
	case EVENT_NAME:
		err = apply_event_name(event, r);
		break;
	case EVENT_AVATAR:
		err = apply_event_avatar(event, r);
		break;
	case EVENT_ENCRYPTION:
		err = apply_event_encryption(event, r);
		break;
	case EVENT_HISTORY_VISIBILITY:
		err = apply_event_history_visibility(event, r);
		break;
	default:
		assert(0);
	}
	if (err)
		return 1;

	return 0;
}
int apply_message_event(const event_t *event, _room_t *r)
{
	int err;
	switch (event->type) {
	case EVENT_MESSAGE:
		err = apply_event_message(event, r);
		break;
	default:
		assert(0);
	}
	if (err)
		return 1;

	return 0;
}
int apply_timeline(_room_t *r, timeline_t *timeline)
{
	for (listentry_t *e = timeline->chunks.next; e != timeline->chunks.prev; e = e->next) {
		event_chunk_t *chunk = list_entry_content(e, event_chunk_t, entry);
		for (listentry_t *f = chunk->events.next; f != &chunk->events; f = f->next) {
			event_t *ev = list_entry_content(e, event_t, entry);
			if (is_statevent(ev->type) && apply_statevent(ev, r))
				return 1;
		}
	}

	event_chunk_t *lastchunk = list_entry_content(timeline->chunks.prev, event_chunk_t, entry);
	assert(lastchunk->type == EVENT_CHUNK_MESSAGE);
	for (listentry_t *e = lastchunk->events.next; e != &lastchunk->events; e = e->next) {
		event_t *ev = list_entry_content(e, event_t, entry);
		if (is_message_event(ev->type)) {
			if (apply_message_event(ev, r))
				return 1;
		} else if (is_statevent(ev->type)) {
			if (apply_statevent(ev, r))
				return 1;
		}
	}

	return 0;
}
int compute_joined_state_from_history(_room_t *r)
{
	room_history_t *history = r->history;

	if (apply_room_summary(r, &history->summary))
		return 1;

	if (apply_timeline(r, &history->timeline))
		return 1;

	if (history->notif_count != -1)
		r->notif_count = history->notif_count;
	if (history->notif_highlight_count != -1)
		r->notif_highlight_count = history->notif_highlight_count;
	return 0;
}
int compute_invited_state_from_history(_room_t *r)
{
	room_history_t *history = r->history;

	if (apply_timeline(r, &history->timeline))
		return 1;

	return 0;
}
int compute_left_state_from_history(_room_t *r)
{
	room_history_t *history = r->history;

	if (apply_timeline(r, &history->timeline))
		return 1;

	return 0;
}

int compute_room_state_from_history(_room_t *r)
{
	if (!r->dirty)
		return 0;

	clear_room_direct_state(r);

	int err;
	switch (r->context) {
	case ROOM_CONTEXT_JOIN:
		err = compute_joined_state_from_history(r);
		break;
	case ROOM_CONTEXT_INVITE:
		err = compute_invited_state_from_history(r);
		break;
	case ROOM_CONTEXT_LEAVE:
		err = compute_left_state_from_history(r);
		break;
	default:
		assert(0);
	}
	if (err)
		return 1;

	r->dirty = 0;
	return 0;
}
