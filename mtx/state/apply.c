#include <assert.h>
#include <string.h>

#include "lib/array.h"
#include "mtx/state/apply.h"

static int unseti(int x)
{
	return x == -1;
}
static int unsets(void *s)
{
	return s == NULL;
}

int apply_event_canonalias(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_canonalias_t *canonalias = event->content;

	if (canonalias->alias && strrpl(&r->canonalias, canonalias->alias))
		return 1;

	if (canonalias->altaliases
			&& strarr_rpl(&r->altaliases, canonalias->altaliases))
		return 1;

	return 0;
}

int apply_event_create(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_create_t *create = event->content;

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

int apply_event_joinrules(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_joinrules_t *joinrules = event->content;

	r->joinrule = joinrules->rule;

	return 0;
}

int apply_event_member(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_member_t *member = event->content;

	mtx_member_t *m = mtx_find_member(&r->members, event->statekey);
	if (!m) {
		m = new_member();
		if (!m)
			return 1;
		mtx_list_add(&r->members, &m->entry);
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

int apply_event_powerlevels(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_powerlevels_t *powerlevels = event->content;

	if (unseti(powerlevels->ban))
		r->powerlevels.ban = powerlevels->ban;

	if (unseti(powerlevels->invite))
		r->powerlevels.invite = powerlevels->invite;

	if (unseti(powerlevels->kick))
		r->powerlevels.kick = powerlevels->kick;

	if (unseti(powerlevels->redact))
		r->powerlevels.redact = powerlevels->redact;

	if (unseti(powerlevels->statedefault))
		r->powerlevels.statedefault = powerlevels->statedefault;

	mtx_list_foreach(&powerlevels->events, mtx_event_powerlevel_t, entry, neweplevel) {
		mtx_event_powerlevel_t *plevel = find_event_powerlevel(
				&r->powerlevels.events, neweplevel->type);
		if (!plevel) {
			plevel = malloc(sizeof(*plevel));
			if (!plevel)
				return 1;
			memset(plevel, 0, sizeof(*plevel));
			mtx_list_add(&r->powerlevels.events, &plevel->entry);
		}
		plevel->type = neweplevel->type;
		plevel->level = neweplevel->level;
	}

	if (unseti(powerlevels->eventdefault))
		r->powerlevels.eventdefault = powerlevels->eventdefault;

	mtx_list_foreach(&powerlevels->users, mtx_user_powerlevel_t, entry, newuplevel) {
		mtx_user_powerlevel_t *plevel = find_user_powerlevel(
				&r->powerlevels.users, newuplevel->id);
		if (!plevel) {
			plevel = malloc(sizeof(*plevel));
			if (!plevel)
				return 1;
			memset(plevel, 0, sizeof(*plevel));
			mtx_list_add(&r->powerlevels.users, &plevel->entry);
		}

		if (strrpl(&plevel->id, newuplevel->id))
			return 1;
		plevel->level = newuplevel->level;
	}

	if (unseti(powerlevels->usersdefault))
		r->powerlevels.usersdefault = powerlevels->usersdefault;

	if (unseti(powerlevels->roomnotif))
		r->powerlevels.roomnotif = powerlevels->roomnotif;

	return 0;
}

int apply_event_name(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_name_t *name = event->content;

	if (name->name && strrpl(&r->name, name->name))
		return 1;

	return 0;
}

int apply_event_topic(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_topic_t *topic = event->content;

	if (topic->topic && strrpl(&r->topic, topic->topic))
		return 1;

	return 0;
}

int apply_event_avatar(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_avatar_t *avatar = event->content;

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

int apply_event_encryption(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_encryption_t *encryption = event->content;

	r->crypt.enabled = 1;

	if (strrpl(&r->crypt.algorithm, encryption->algorithm))
		return 1;

	r->crypt.rotmsgnum = encryption->rotmsgnum;
	r->crypt.rotperiod = encryption->rotperiod;
	return 0;
}

int apply_event_encrypted(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_encrypted_t *encrypted = event->content;

	assert(0);
	return 0;
}

int apply_event_room_key_request(const mtx_event_t *event)
{
	mtx_ev_room_key_request_t *request = event->content;

	assert(0);
	return 0;
}

int apply_event_history_visibility(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_history_visibility_t *histvisib = event->content;

	r->histvisib = histvisib->visib;
	return 0;
}

int apply_event_guest_access(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_guest_access_t *guestaccess = event->content;

	r->guestaccess = guestaccess->access;
	return 0;
}

int apply_event_message(const mtx_event_t *event, mtx_room_t *r)
{
	mtx_ev_message_t *msg = event->content;

	mtx_msg_t *m = malloc(sizeof(*m));
	if (!m)
		return 1;
	memset(m, 0, sizeof(*m));
	mtx_list_add(&r->msgs, &m->entry);

	m->type = msg->type;
	if (strrpl(&m->body, msg->body))
		return 1;

	if (strrpl(&m->sender, event->sender))
		return 1;

	void *content = dup_message_content(msg->type, msg->content);
	if (!content)
		return 1;
	m->content = content;

	return 0;
}

int apply_room_summary(mtx_room_t *r, mtx_room_summary_t *summary)
{
	if (summary->heroes && strarr_rpl(&r->heroes, summary->heroes))
		return 1;

	if (r->ninvited != -1)
		r->ninvited = summary->ninvited;
	if (r->njoined != -1)
		r->njoined = summary->njoined;

	return 0;
}
int apply_statevent(const mtx_event_t *event, mtx_room_t *r)
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
	case EVENT_TOPIC:
		err = apply_event_topic(event, r);
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
	case EVENT_GUEST_ACCESS:
		err = apply_event_guest_access(event, r);
		break;
	default:
		assert(0);
	}
	if (err)
		return 1;

	return 0;
}
int apply_message_event(const mtx_event_t *event, mtx_room_t *r)
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
int apply_timeline(mtx_room_t *r, mtx_timeline_t *timeline)
{
	for (mtx_listentry_t *e = timeline->chunks.next; e != timeline->chunks.prev; e = e->next) {
		mtx_event_chunk_t *chunk = mtx_list_entry_content(e, mtx_event_chunk_t, entry);
		for (mtx_listentry_t *f = chunk->events.next; f != &chunk->events; f = f->next) {
			mtx_event_t *ev = mtx_list_entry_content(e, mtx_event_t, entry);
			if (is_statevent(ev->type) && apply_statevent(ev, r))
				return 1;
		}
	}

	mtx_event_chunk_t *lastchunk = mtx_list_entry_content(
			timeline->chunks.prev, mtx_event_chunk_t, entry);
	assert(lastchunk->type == EVENT_CHUNK_MESSAGE);
	mtx_list_foreach(&lastchunk->events, mtx_event_t, entry, ev) {
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
int compute_joined_state_from_history(mtx_room_t *r)
{
	mtx_room_history_t *history = r->history;

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
int compute_invited_state_from_history(mtx_room_t *r)
{
	mtx_room_history_t *history = r->history;

	if (apply_timeline(r, &history->timeline))
		return 1;

	return 0;
}
int compute_left_state_from_history(mtx_room_t *r)
{
	mtx_room_history_t *history = r->history;

	if (apply_timeline(r, &history->timeline))
		return 1;

	return 0;
}

int compute_room_state_from_history(mtx_room_t *r)
{
	if (!r->dirty)
		return 0;

	clear_room_direct_state(r);

	int err;
	switch (r->context) {
	case MTX_ROOM_CONTEXT_JOIN:
		err = compute_joined_state_from_history(r);
		break;
	case MTX_ROOM_CONTEXT_INVITE:
		err = compute_invited_state_from_history(r);
		break;
	case MTX_ROOM_CONTEXT_LEAVE:
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

int apply_to_device_events(const mtx_listentry_t *events)
{
	assert(0);
	return 0;
}
