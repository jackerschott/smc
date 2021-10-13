#include <assert.h>
#include <string.h>

#include <json-c/json.h>

#include "lib/array.h"
#include "lib/hjson.h"
#include "mtx/state/room.h"

mtx_event_t *dup_event(mtx_event_t *event);

/* history types */
void free_ev_canonalias(mtx_ev_canonalias_t *canonalias)
{
	if (!canonalias)
		return;

	free(canonalias->alias);
	for (size_t i = 0; canonalias->altaliases[i] != NULL; ++i) {
		free(canonalias->altaliases[i]);
	}
	free(canonalias);
}
mtx_ev_canonalias_t *dup_ev_canonalias(mtx_ev_canonalias_t *canonalias)
{
	assert(canonalias);

	mtx_ev_canonalias_t *ca = malloc(sizeof(*ca));
	if (!ca)
		return NULL;
	memset(ca, 0, sizeof(*ca));

	if (strrpl(&ca->alias, canonalias->alias))
		goto err_free_canonalias;

	if (strarr_rpl(&ca->altaliases, canonalias->altaliases))
		goto err_free_canonalias;

	return ca;

err_free_canonalias:
	free_ev_canonalias(ca);
	return NULL;
}

void free_ev_create(mtx_ev_create_t *create)
{
	if (!create)
		return;

	free(create->creator);
	free(create->version);
	free(create->previd);
	free(create->prev_last_eventid);
	free(create);
}
mtx_ev_create_t *dup_ev_create(mtx_ev_create_t *create)
{
	assert(create);

	mtx_ev_create_t *c = malloc(sizeof(*c));
	if (!c)
		return NULL;
	memset(c, 0, sizeof(*c));

	if (strrpl(&c->creator, create->creator))
		goto err_free_create;

	c->federate = create->federate;

	if (strrpl(&c->version, create->version))
		goto err_free_create;

	if (strrpl(&c->previd, create->previd))
		goto err_free_create;

	if (strrpl(&c->prev_last_eventid, create->prev_last_eventid))
		goto err_free_create;

	return c;

err_free_create:
	free_ev_create(c);
	return NULL;
}

void free_ev_joinrules(mtx_ev_joinrules_t *joinrules)
{
	if (!joinrules)
		return;

	free(joinrules);
}
mtx_ev_joinrules_t *dup_ev_joinrules(mtx_ev_joinrules_t *joinrules)
{
	assert(joinrules);

	mtx_ev_joinrules_t *rule = malloc(sizeof(*rule));
	if (!rule)
		return NULL;
	memset(rule, 0, sizeof(*rule));

	rule->rule = joinrules->rule;

	return rule;
}

void free_ev_member(mtx_ev_member_t *member)
{
	if (!member)
		return;

	free(member->avatarurl);
	free(member->displayname);
	mtx_list_free(&member->invite_room_events, mtx_event_t, entry, free_event);

	mtx_thirdparty_invite_t tpinvite = member->thirdparty_invite;
	free(tpinvite.displayname);
	free(tpinvite.mxid);
	json_object_put(tpinvite.signatures);
	free(tpinvite.token);

	free(member);
}
mtx_ev_member_t *dup_ev_member(mtx_ev_member_t *member)
{
	assert(member);

	mtx_ev_member_t *m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(*m));

	if (strrpl(&m->avatarurl, member->avatarurl))
		goto err_free_member;

	if (strrpl(&m->displayname, member->displayname))
		goto err_free_member;

	m->membership = member->membership;

	m->isdirect = member->isdirect;

	mtx_thirdparty_invite_t *invite = &member->thirdparty_invite;
	mtx_thirdparty_invite_t *newinvite = &m->thirdparty_invite;
	if (strrpl(&newinvite->displayname, invite->displayname))
		goto err_free_member;

	if (strrpl(&newinvite->mxid, invite->mxid))
		goto err_free_member;

	if (invite->signatures && json_object_deep_copy(invite->signatures,
				&newinvite->signatures, NULL))
		goto err_free_member;

	if (strrpl(&newinvite->token, invite->token))
		goto err_free_member;

	mtx_listentry_t *events = &m->invite_room_events;
	mtx_list_dup(events, &member->invite_room_events, mtx_event_t, entry, dup_event);
	if (!events)
		goto err_free_member;

	return m;

err_free_member:
	free_ev_member(m);
	return NULL;
}

void free_event_powerlevel(mtx_event_powerlevel_t *plevel)
{
	if (!plevel)
		return;

	free(plevel);
}
mtx_event_powerlevel_t *dup_event_powerlevel(mtx_event_powerlevel_t *plevel)
{
	mtx_event_powerlevel_t *p = malloc(sizeof(*p));
	if (!p)
		return NULL;
	memset(p, 0, sizeof(*p));

	p->type = plevel->type;
	p->level = plevel->level;

	return p;
}
mtx_event_powerlevel_t *find_event_powerlevel(const mtx_listentry_t *plevels, mtx_eventtype_t type)
{
	for (mtx_listentry_t *e = plevels->next; e != plevels; e = e->next) {
		mtx_event_powerlevel_t *plevel = mtx_list_entry_content(e, mtx_event_powerlevel_t, entry);
		if (plevel->type == type)
			return plevel;
	}

	return NULL;
}
void free_user_powerlevel(mtx_user_powerlevel_t *plevel)
{
	if (!plevel)
		return;

	free(plevel->id);
	free(plevel);
}
mtx_user_powerlevel_t *dup_user_powerlevel(mtx_user_powerlevel_t *plevel)
{
	mtx_user_powerlevel_t *p = malloc(sizeof(*p));
	if (!p)
		return NULL;
	memset(p, 0, sizeof(*p));

	if (strrpl(&p->id, plevel->id))
		goto err_free_powerlevel;
	p->level = plevel->level;

	return p;

err_free_powerlevel:
	free_user_powerlevel(p);
	return NULL;
}
mtx_user_powerlevel_t *find_user_powerlevel(const mtx_listentry_t *plevels, const char *id)
{
	for (mtx_listentry_t *e = plevels->next; e != plevels; e = e->next) {
		mtx_user_powerlevel_t *plevel = mtx_list_entry_content(e, mtx_user_powerlevel_t, entry);
		if (strcmp(plevel->id, id) == 0)
			return plevel;
	}

	return NULL;
}
void free_ev_powerlevels(mtx_ev_powerlevels_t *powerlevels)
{
	if (!powerlevels)
		return;

	mtx_list_free(&powerlevels->events, mtx_event_powerlevel_t, entry, free_event_powerlevel);
	mtx_list_free(&powerlevels->users, mtx_user_powerlevel_t, entry, free_user_powerlevel);
	free(powerlevels);
}
mtx_ev_powerlevels_t *dup_ev_powerlevels(mtx_ev_powerlevels_t *powerlevels)
{
	assert(powerlevels);
	
	mtx_ev_powerlevels_t *levels = malloc(sizeof(*levels));
	if (!levels)
		return NULL;
	memset(levels, 0, sizeof(*levels));

	levels->ban = powerlevels->ban;
	levels->invite = powerlevels->invite;
	levels->kick = powerlevels->kick;
	levels->redact = powerlevels->redact;
	levels->statedefault = powerlevels->statedefault;

	mtx_listentry_t *events = &levels->events;
	mtx_list_dup(events, &powerlevels->events, mtx_event_powerlevel_t, entry, dup_event_powerlevel);
	if (!events)
		goto err_free_powerlevels;
	levels->eventdefault = powerlevels->eventdefault;

	mtx_listentry_t *users = &levels->users;
	mtx_list_dup(users, &powerlevels->users, mtx_user_powerlevel_t, entry, dup_user_powerlevel);
	if (!users)
		goto err_free_powerlevels;
	levels->usersdefault = powerlevels->usersdefault;

	levels->roomnotif = powerlevels->roomnotif;

	return levels;

err_free_powerlevels:
	free_ev_powerlevels(levels);
	return NULL;
}

void free_ev_redaction(mtx_ev_redaction_t *redaction)
{
	if (!redaction)
		return;

	free(redaction->reason);
	free(redaction);
}
mtx_ev_redaction_t *dup_ev_redaction(mtx_ev_redaction_t *redaction)
{
	assert(redaction);

	mtx_ev_redaction_t *r = malloc(sizeof(*r));
	if (!r)
		return NULL;
	memset(r, 0, sizeof(*r));

	if (strrpl(&r->reason, redaction->reason))
		goto err_free_redaction;

	return r;

err_free_redaction:
	free_ev_redaction(r);
	return NULL;
}

void free_ev_name(mtx_ev_name_t *name)
{
	if (!name)
		return;

	free(name->name);
	free(name);
}
mtx_ev_name_t *dup_ev_name(mtx_ev_name_t *name)
{
	assert(name);

	mtx_ev_name_t *n = malloc(sizeof(*n));
	if (!n)
		return NULL;
	memset(n, 0, sizeof(*n));

	if (strrpl(&n->name, name->name))
		goto err_free_name;

	return n;

err_free_name:
	free_ev_name(n);
	return NULL;
}

void free_ev_topic(mtx_ev_topic_t *topic)
{
	if (!topic)
		return;

	free(topic->topic);
	free(topic);
}
mtx_ev_topic_t *dup_ev_topic(mtx_ev_topic_t *topic)
{
	assert(topic);

	mtx_ev_topic_t *n = malloc(sizeof(*n));
	if (!n)
		return NULL;
	memset(n, 0, sizeof(*n));

	if (strrpl(&n->topic, topic->topic))
		goto err_free_topic;

	return n;

err_free_topic:
	free_ev_topic(n);
	return NULL;
}

void free_ev_avatar(mtx_ev_avatar_t *avatar)
{
	if (!avatar)
		return;

	free(avatar->url);
	free(avatar->info.mimetype);

	free(avatar->thumburl);
	free(avatar->thumbinfo.mimetype);
	free(avatar);
}
mtx_ev_avatar_t *dup_ev_avatar(mtx_ev_avatar_t *avatar)
{
	assert(avatar);

	mtx_ev_avatar_t *av = malloc(sizeof(*av));
	if (!av)
		return NULL;
	memset(av, 0, sizeof(*av));

	if (strrpl(&av->url, avatar->url))
		goto err_free_avatar;

	av->info.w = avatar->info.w;
	av->info.h = avatar->info.h;
	av->info.size = avatar->info.size;
	if (strrpl(&av->info.mimetype, avatar->info.mimetype))
		goto err_free_avatar;

	if (strrpl(&av->thumburl, avatar->thumburl))
		goto err_free_avatar;

	av->thumbinfo.w = avatar->thumbinfo.w;
	av->thumbinfo.h = avatar->thumbinfo.h;
	av->thumbinfo.size = avatar->thumbinfo.size;
	if (strrpl(&av->thumbinfo.mimetype, avatar->thumbinfo.mimetype))
		goto err_free_avatar;

	return av;

err_free_avatar:
	free_ev_avatar(av);
	return NULL;
}

void free_ev_encryption(mtx_ev_encryption_t *encryption)
{
	free(encryption->algorithm);
	free(encryption);
}
mtx_ev_encryption_t *dup_ev_encryption(mtx_ev_encryption_t *encryption)
{
	assert(encryption);

	mtx_ev_encryption_t *crypt = malloc(sizeof(*crypt));
	if (!crypt)
		return NULL;
	memset(crypt, 0, sizeof(*crypt));

	if (strrpl(&crypt->algorithm, encryption->algorithm))
		goto err_free_encryption;
	crypt->rotperiod = encryption->rotperiod;
	crypt->rotmsgnum = encryption->rotmsgnum;

	return crypt;

err_free_encryption:
	free_ev_encryption(crypt);
	return NULL;
}

int format_ciphertext_info(json_object *obj, const mtx_ciphertext_info_t *info)
{
	if (json_add_string_(obj, "body", info->body))
		return 1;

	if (json_add_int_(obj, "type", info->type))
		return 1;

	return 0;
}

void free_ciphertext_info(mtx_ciphertext_info_t *info)
{
	if (!info)
		return;

	free(info->identkey);
	free(info->body);
	free(info);
}
void free_ev_encrypted(mtx_ev_encrypted_t *encrypted)
{
	if (!encrypted)
		return;

	const char *algolm = mtx_crypt_algorithm_strs[MTX_CRYPT_ALGORITHM_OLM];
	const char *algmegolm = mtx_crypt_algorithm_strs[MTX_CRYPT_ALGORITHM_MEGOLM];
	if (strcmp(encrypted->algorithm, algolm) == 0) {
		mtx_list_free(&encrypted->olm.ciphertext,
				mtx_ciphertext_info_t, entry, free_ciphertext_info);
	} else if (strcmp(encrypted->algorithm, algmegolm) == 0) {
		free(encrypted->megolm.ciphertext);
		free(encrypted->megolm.deviceid);
		free(encrypted->megolm.sessionid);
	} else {
		assert(0);
	}

	free(encrypted->senderkey);
	free(encrypted->algorithm);
	free(encrypted);
}
mtx_ev_encrypted_t *dup_ev_encrypted(mtx_ev_encrypted_t *encrypted)
{
	assert(0);
}
json_object *format_ev_encrypted(const mtx_ev_encrypted_t *encrypted)
{
	json_object *obj = json_object_new_object();
	if (!obj)
		return NULL;

	if (json_add_string_(obj, "algorithm", encrypted->algorithm))
		goto err_free_obj;

	if (json_add_string_(obj, "sender_key", encrypted->senderkey))
		goto err_free_obj;

	const char *algolm = mtx_crypt_algorithm_strs[MTX_CRYPT_ALGORITHM_OLM];
	const char *algmegolm = mtx_crypt_algorithm_strs[MTX_CRYPT_ALGORITHM_MEGOLM];
	if (strcmp(encrypted->algorithm, algolm) == 0) {
		json_object *ciphertext;
		if (json_add_object_(obj, "ciphertext", &ciphertext))
			goto err_free_obj;

		mtx_list_foreach(&encrypted->olm.ciphertext, mtx_ciphertext_info_t, entry, info) {
			json_object *_info;
			if (json_add_object_(obj, info->identkey, &_info))
				goto err_free_obj;

			if (format_ciphertext_info(_info, info))
				goto err_free_obj;
		}
	} else if (strcmp(encrypted->algorithm, algmegolm) == 0) {
		if (json_add_string_(obj, "ciphertext", encrypted->megolm.ciphertext))
			goto err_free_obj;

		if (json_add_string_(obj, "device_id", encrypted->megolm.deviceid))
			goto err_free_obj;

		if (json_add_string_(obj, "session_id", encrypted->megolm.sessionid))
			goto err_free_obj;
	} else {
		assert(0);
	}

	return obj;

err_free_obj:
	json_object_put(obj);
	return obj;
}

void free_ev_room_key_request(mtx_ev_room_key_request_t *request)
{
	if (!request)
		return;

	free(request->body.algorithm);
	free(request->body.roomid);
	free(request->body.senderkey);
	free(request->body.sessionid);
	free(request->deviceid);
	free(request->requestid);
	free(request);
}
json_object *format_ev_room_key_request(const mtx_ev_room_key_request_t *keyrequest)
{
	json_object *obj = json_object_new_object();
	if (!obj)
		return NULL;

	json_object *body;
	if (json_add_object_(obj, "body", &body))
		goto err_free_obj;

	if (json_add_string_(body, "algorithm", keyrequest->body.algorithm))
		goto err_free_obj;

	if (json_add_string_(body, "room_id", keyrequest->body.algorithm))
		goto err_free_obj;

	if (json_add_string_(body, "sender_key", keyrequest->body.algorithm))
		goto err_free_obj;

	if (json_add_string_(body, "session_id", keyrequest->body.algorithm))
		goto err_free_obj;

	if (json_add_enum_(obj, "action", keyrequest->action, mtx_key_request_action_strs))
		goto err_free_obj;

	if (json_add_string_(obj, "requesting_device_id", keyrequest->deviceid))
		goto err_free_obj;

	if (json_add_string_(obj, "request_id", keyrequest->requestid))
		goto err_free_obj;

	return obj;

err_free_obj:
	json_object_put(obj);
	return NULL;
}

void free_ev_history_visibility(mtx_ev_history_visibility_t *visib)
{
	if (!visib)
		return;

	free(visib);
}
mtx_ev_history_visibility_t *dup_ev_history_visibility(mtx_ev_history_visibility_t *visib)
{
	assert(visib);

	mtx_ev_history_visibility_t *v = malloc(sizeof(*v));
	if (!v)
		return NULL;
	memset(v, 0, sizeof(*v));

	v->visib = visib->visib;

	return v;
}

void free_ev_guest_access(mtx_ev_guest_access_t *guestaccess)
{
	if (!guestaccess)
		return;

	free(guestaccess);
}
mtx_ev_guest_access_t *dup_ev_guest_access(mtx_ev_guest_access_t *guestaccess)
{
	assert(guestaccess);
	
	mtx_ev_guest_access_t *ga = malloc(sizeof(*ga));
	if (!ga)
		return NULL;
	memset(ga, 0, sizeof(*ga));

	ga->access = guestaccess->access;

	return ga;
}

void free_message_text(mtx_message_text_t *msg)
{
	if (!msg)
		return;

	free(msg->fmt);
	free(msg->fmtbody);
	free(msg);
}
mtx_message_text_t *dup_message_text(mtx_message_text_t *msg)
{
	if (!msg)
		return NULL;

	mtx_message_text_t *m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(*m));

	if (strrpl(&m->fmt, msg->fmt))
		goto err_free_msg;

	if (strrpl(&m->fmtbody, msg->fmtbody))
		goto err_free_msg;

	return m;

err_free_msg:
	free_message_text(m);
	return NULL;
}
int format_message_text(json_object *obj, const mtx_message_text_t *m)
{
	if (json_add_string_(obj, "format", m->fmt))
		return 1;

	if (json_add_string_(obj, "formatted_body", m->fmtbody))
		return 1;

	return 0;
}

void free_message_emote(mtx_message_emote_t *msg)
{
	if (!msg)
		return;

	free(msg->fmt);
	free(msg->fmtbody);
	free(msg);
}
mtx_message_emote_t *dup_message_emote(mtx_message_emote_t *msg)
{
	if (!msg)
		return NULL;

	mtx_message_emote_t *m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(*m));

	if (strrpl(&m->fmt, msg->fmt))
		goto err_free_msg;

	if (strrpl(&m->fmtbody, msg->fmtbody))
		goto err_free_msg;

	return m;

err_free_msg:
	free_message_emote(m);
	return NULL;

}
int format_message_emote(json_object *obj, const mtx_message_emote_t *m)
{
	if (json_add_string_(obj, "format", m->fmt))
		return 1;

	if (json_add_string_(obj, "formatted_body", m->fmtbody))
		return 1;

	return 0;
}

void free_message_content(mtx_msg_type_t type, void *content)
{
	if (!content)
		return;

	switch (type) {
	case MTX_MSG_TEXT:
		free_message_text(content);
		break;
	case MTX_MSG_EMOTE:
		free_message_emote(content);
		break;
	default:
		assert(0);
	}
}
void *dup_message_content(mtx_msg_type_t type, void *content)
{
	assert(content);

	void *c;
	switch (type) {
	case MTX_MSG_TEXT:
		c = dup_message_text(content);
		break;
	case MTX_MSG_EMOTE:
		c = dup_message_emote(content);
		break;
	default:
		assert(0);
	}
	if (!c)
		return NULL;

	return c;
}
int format_message_content(mtx_msg_type_t type, json_object *obj, const void *content)
{
	assert(content);

	int err;
	switch (type) {
	case MTX_MSG_TEXT:
		err = format_message_text(obj, content);
		break;
	case MTX_MSG_EMOTE:
		err = format_message_emote(obj, content);
		break;
	default:
		assert(0);
	}
	if (err)
		return 1;

	return 0;
}

void free_ev_message(mtx_ev_message_t *msg)
{
	if (!msg)
		return;

	free(msg->body);
	free_message_content(msg->type, msg->content);
	free(msg);
}
mtx_ev_message_t *dup_ev_message(mtx_ev_message_t *msg)
{
	if (!msg)
		return NULL;

	mtx_ev_message_t *m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(*m));

	m->type = msg->type;

	if (strrpl(&m->body, msg->body))
		goto err_free_message;

	assert(msg->content);
	void *content = dup_message_content(msg->type, msg->content);
	if (!content)
		goto err_free_message;
	m->content = content;

	return m;

err_free_message:
	free_ev_message(m);
	return NULL;
}
json_object *format_ev_message(const mtx_ev_message_t *msg)
{
	json_object *obj = json_object_new_object();
	if (!obj)
		return NULL;

	if (json_add_string_(obj, "msgtype", mtx_msg_type_strs[msg->type]))
		goto err_free_obj;

	if (json_add_string_(obj, "body", msg->body))
		goto err_free_obj;

	return obj;

err_free_obj:
	json_object_put(obj);
	return NULL;
}

void free_event_content(mtx_eventtype_t type, void *content)
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
		free_ev_member(content);
		break;
	case EVENT_POWERLEVELS:
		free_ev_powerlevels(content);
		break;
	case EVENT_NAME:
		free_ev_name(content);
		break;
	case EVENT_TOPIC:
		free_ev_topic(content);
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
	case EVENT_GUEST_ACCESS:
		free_ev_guest_access(content);
		break;
	case EVENT_REDACTION:
		free_ev_redaction(content);
		break;
	case EVENT_MESSAGE:
		free_ev_message(content);
		break;
	default:
		assert(0);
	}
}
void free_event(mtx_event_t *event)
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
void free_event_chunk(mtx_event_chunk_t *chunk)
{
	if (!chunk)
		return;

	mtx_list_free(&chunk->events, mtx_event_t, entry, free_event);
	free(chunk);
}
void *dup_event_content(mtx_eventtype_t type, void *content)
{
	assert(content);

	void *c;
	switch(type) {
	case EVENT_CANONALIAS:
		c = dup_ev_canonalias(content);
		break;
	case EVENT_CREATE:
		c = dup_ev_create(content);
		break;
	case EVENT_JOINRULES:
		c = dup_ev_joinrules(content);
		break;
	case EVENT_MEMBER:
		c = dup_ev_member(content);
		break;
	case EVENT_POWERLEVELS:
		c = dup_ev_powerlevels(content);
		break;
	case EVENT_NAME:
		c = dup_ev_name(content);
		break;
	case EVENT_TOPIC:
		c = dup_ev_topic(content);
		break;
	case EVENT_AVATAR:
		c = dup_ev_avatar(content);
		break;
	case EVENT_ENCRYPTION:
		c = dup_ev_encryption(content);
		break;
	case EVENT_HISTORY_VISIBILITY:
		c = dup_ev_history_visibility(content);
		break;
	case EVENT_GUEST_ACCESS:
		c = dup_ev_guest_access(content);
		break;
	case EVENT_REDACTION:
		c = dup_ev_redaction(content);
		break;
	case EVENT_MESSAGE:
		c = dup_ev_message(content);
		break;
	default:
		assert(0);
	}
	if (!c)
		return NULL;

	return c;
}
mtx_event_t *dup_event(mtx_event_t *event)
{
	assert(event);

	mtx_event_t *ev = malloc(sizeof(*ev));
	if (!ev)
		return NULL;
	memset(ev, 0, sizeof(*ev));

	ev->type = event->type;

	if (strrpl(&ev->id, event->id))
		goto err_free_event;

	if (strrpl(&ev->sender, event->sender))
		goto err_free_event;

	ev->ts = event->ts;

	ev->age = event->age;

	if (event->redactreason) {
		mtx_event_t *redactreason = dup_event(event->redactreason);
		if (!redactreason)
			goto err_free_event;
		ev->redactreason = redactreason;
	}

	if (strrpl(&ev->txnid, event->txnid))
		goto err_free_event;

	if (strrpl(&ev->roomid, event->roomid))
		goto err_free_event;

	if (strrpl(&ev->statekey, event->statekey))
		goto err_free_event;

	if (strrpl(&ev->redacts, event->redacts))
		goto err_free_event;

	if (event->prevcontent) {
		void *prevcontent = dup_event_content(event->type, event->prevcontent);
		if (!prevcontent)
			goto err_free_event;
		ev->prevcontent = event->prevcontent;
	}

	if (event->content) {
		void *content = dup_event_content(event->type, event->content);
		if (!content)
			goto err_free_event;
		ev->content = content;
	}

	return ev;

err_free_event:
	free_event(ev);
	return NULL;
}
mtx_event_chunk_t *dup_event_chunk(mtx_event_chunk_t *chunk)
{
	assert(chunk);

	mtx_event_chunk_t *c = malloc(sizeof(*c));
	if (!c)
		return NULL;
	memset(c, 0, sizeof(*c));
	mtx_list_init(&c->events);

	c->type = chunk->type;

	mtx_listentry_t *events = &c->events;
	mtx_list_dup(events, &chunk->events, mtx_event_t, entry, dup_event);
	if (!events)
		goto err_free_chunk;

	return c;

err_free_chunk:
	free_event_chunk(c);
	return NULL;
}
int is_roomevent(mtx_eventtype_t type)
{
	return EVENT_CANONALIAS <= type && type <= EVENT_MESSAGE;
}
int is_statevent(mtx_eventtype_t type)
{
	return EVENT_CANONALIAS <= type && type <= EVENT_TOMBSTONE;
}
int is_message_event(mtx_eventtype_t type)
{
	return EVENT_REDACTION <= type && type <= EVENT_MESSAGE;
}
mtx_event_t *find_event(mtx_listentry_t *chunks, const char *eventid)
{
	for (mtx_listentry_t *e = chunks->next; e != chunks; e = e->next) {
		mtx_event_chunk_t *chunk = mtx_list_entry_content(e, mtx_event_chunk_t, entry);
		for (mtx_listentry_t *f = chunk->events.next; f != &chunk->events; f = f->next) {
			mtx_event_t *ev = mtx_list_entry_content(f, mtx_event_t, entry);
			if (ev->id != eventid)
				continue;

			return ev;
		}
	}
	return NULL;
}

void free_room_history(mtx_room_history_t *history)
{
	if (!history)
		return;

	if (history->summary.heroes) {
		for (size_t i = 0; history->summary.heroes[i]; ++i) {
			free(history->summary.heroes[i]);
		}
	}

	mtx_list_free(&history->timeline.chunks, mtx_event_chunk_t, entry, free_event_chunk);
	free(history->timeline.prevbatch);

	mtx_list_free(&history->ephemeral, mtx_event_t, entry, free_event);
	mtx_list_free(&history->account, mtx_event_t, entry, free_event);

	free(history);
}
void free_room_history_context(mtx_room_t *r)
{
	if (!r)
		return;

	free(r->id);
	free_room_history(r->history);
}
mtx_room_history_t *new_room_history(void)
{
	mtx_room_history_t *history = malloc(sizeof(*history));
	if (!history)
		return NULL;
	memset(history, 0, sizeof(*history));

	mtx_list_init(&history->timeline.chunks);
	mtx_list_init(&history->ephemeral);
	mtx_list_init(&history->account);

	return history;
}
mtx_room_history_t *dup_history(mtx_room_history_t *history)
{
	if (!history)
		return NULL;

	mtx_room_history_t *h = malloc(sizeof(*h));
	if (!h)
		return NULL;
	memset(h, 0, sizeof(*h));

	if (strarr_rpl(&h->summary.heroes, history->summary.heroes))
		goto err_free_history;
	h->summary.njoined = history->summary.njoined;
	h->summary.ninvited = history->summary.ninvited;

	h->timeline.limited = history->timeline.limited;
	if (strrpl(&h->timeline.prevbatch, history->timeline.prevbatch))
		goto err_free_history;

	mtx_listentry_t *chunks = &h->timeline.chunks;
	mtx_list_dup(chunks, &history->timeline.chunks, mtx_event_chunk_t, entry, dup_event_chunk);
	if (!chunks)
		goto err_free_history;

	// TODO: copy account and ephemeral

	h->notif_count = history->notif_count;
	h->notif_highlight_count = history->notif_highlight_count;

	return h;

err_free_history:
	free_room_history(h);
	return NULL;
}
mtx_room_t *find_room(mtx_listentry_t *rooms, const char *id)
{
	for (mtx_listentry_t *e = rooms->next; e != rooms; e = e->next) {
		mtx_room_t *r = mtx_list_entry_content(e, mtx_room_t, entry);

		if (strcmp(r->id, id) == 0)
			return r;
	}
	return NULL;
}

/* direct state types */
void free_member(mtx_member_t *m)
{
	if (!m)
		return;

	free(m->userid);
	free(m->displayname);
	free(m->avatarurl);
	free(m);
}
mtx_member_t *new_member(void)
{
	mtx_member_t *m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(*m));
	m->membership = MEMBERSHIP_NUM;
	m->isdirect = -1;
	
	return m;
}
mtx_member_t *dup_member(mtx_member_t *member)
{
	assert(member);

	mtx_member_t *m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(*m));

	if (strrpl(&m->userid, member->userid))
		goto err_free_member;

	m->membership = member->membership;

	if (strrpl(&m->displayname, member->displayname))
		goto err_free_member;

	if (strrpl(&m->avatarurl, member->avatarurl))
		goto err_free_member;

	m->isdirect = member->isdirect;

	return m;

err_free_member:
	free_member(m);
	return NULL;
}
mtx_member_t *mtx_find_member(mtx_listentry_t *members, const char *userid)
{
	mtx_list_foreach(members, mtx_member_t, entry, m) {
		if (strcmp(m->userid, userid) == 0)
			return m;
	}
	return NULL;
}

void free_msg(mtx_msg_t *m)
{
	if (!m)
		free(m);

	free(m->body);
	free(m->sender);
	free_message_content(m->type, m->content);
	free(m);
}
mtx_msg_t *dup_msg(mtx_msg_t *msg)
{
	if (!msg)
		return NULL;

	mtx_msg_t *m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(*m));

	m->type = msg->type;

	if (strrpl(&m->sender, msg->sender))
		goto err_free_msg;

	if (strrpl(&m->body, msg->body))
		goto err_free_msg;

	assert(msg->content);
	void *content = dup_message_content(msg->type, msg->content);
	if (!content)
		goto err_free_msg;
	m->content = content;

	return m;

err_free_msg:
	free_msg(m);
	return NULL;
}

void free_room_direct_state_context(mtx_room_t *r)
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
	mtx_list_free(&r->members, mtx_member_t, entry, free_member);

	mtx_list_free(&r->powerlevels.events, mtx_event_powerlevel_t, entry, free_event_powerlevel);
	mtx_list_free(&r->powerlevels.users, mtx_user_powerlevel_t, entry, free_user_powerlevel);

	free(r->avatarurl);
	free(r->avatarinfo.mimetype);
	free(r->avatarthumburl);
	free(r->avatarthumbinfo.mimetype);

	mtx_list_free(&r->msgs, mtx_msg_t, entry, free_msg);

	free(r->crypt.algorithm);
}

/* general */
void mtx_free_room(mtx_room_t *r)
{
	free_room_direct_state_context(r);
	free_room_history_context(r);
	free(r);
}
mtx_room_t *new_room(const char *id, mtx_room_context_t context)
{
	mtx_room_t *r = malloc(sizeof(*r));
	if (!r)
		return NULL;
	memset(r, 0, sizeof(*r));

	r->ninvited = 0;
	r->njoined = 0;
	r->notif_count = 0;
	r->notif_highlight_count = 0;

	mtx_list_init(&r->members);

	r->powerlevels.ban = 50;
	r->powerlevels.invite = 50;
	r->powerlevels.kick = 50;
	r->powerlevels.redact = 50;
	r->powerlevels.statedefault = 50;

	mtx_list_init(&r->powerlevels.events);
	r->powerlevels.eventdefault = 0;

	mtx_list_init(&r->powerlevels.users);
	r->powerlevels.usersdefault = 0;

	r->powerlevels.roomnotif = 50;

	r->joinrule = JOINRULE_NUM;

	r->histvisib = HISTVISIB_NUM;

	mtx_list_init(&r->msgs);

	if (strrpl(&r->id, id)) {
		mtx_free_room(r);
		return NULL;
	}

	r->context = context;

	mtx_room_history_t *history = new_room_history();
	if (!history) {
		mtx_free_room(r);
		return NULL;
	}
	r->history = history;

	r->dirty = 1;
	return r;
}
mtx_room_t *mtx_dup_room(mtx_room_t *room)
{
	mtx_room_t *r = malloc(sizeof(*r));
	if (!r)
		return NULL;
	memset(r, 0, sizeof(*r));

	if (strrpl(&r->id, room->id))
		goto err_free_room;

	if (strrpl(&r->creator, room->creator))
		goto err_free_room;

	r->federate = room->federate;

	if (strrpl(&r->previd, room->previd))
		goto err_free_room;

	if (strrpl(&r->prev_last_eventid, room->prev_last_eventid))
		goto err_free_room;

	if (strrpl(&r->name, room->name))
		goto err_free_room;

	if (strrpl(&r->topic, room->topic))
		goto err_free_room;

	if (strrpl(&r->canonalias, room->canonalias))
		goto err_free_room;

	if (strarr_rpl(&r->altaliases, room->altaliases))
		goto err_free_room;
	
	if (strarr_rpl(&r->heroes, room->heroes))
		goto err_free_room;

	r->ninvited = room->ninvited;
	r->njoined = room->njoined;
	r->notif_count = room->notif_count;
	r->notif_highlight_count = room->notif_highlight_count;

	mtx_listentry_t *members = &r->members;
	mtx_list_dup(members, &room->members, mtx_member_t, entry, dup_member);
	if (!members)
		goto err_free_room;

	r->powerlevels.ban = room->powerlevels.ban;
	r->powerlevels.invite = room->powerlevels.invite;
	r->powerlevels.kick = room->powerlevels.kick;
	r->powerlevels.redact = room->powerlevels.redact;
	r->powerlevels.statedefault = room->powerlevels.statedefault;

	mtx_listentry_t *evpowerlevels = &r->powerlevels.events;
	mtx_list_dup(evpowerlevels, &room->powerlevels.events,
			mtx_event_powerlevel_t, entry, dup_event_powerlevel);
	if (!evpowerlevels)
		goto err_free_room;
	r->powerlevels.eventdefault = room->powerlevels.eventdefault;

	mtx_listentry_t *userpowerlevels = &r->powerlevels.users;
	mtx_list_dup(userpowerlevels, &room->powerlevels.users,
			mtx_user_powerlevel_t, entry, dup_user_powerlevel);
	if (!userpowerlevels)
		goto err_free_room;
	r->powerlevels.usersdefault = room->powerlevels.usersdefault;

	r->powerlevels.roomnotif = room->powerlevels.roomnotif;

	r->joinrule = room->joinrule;
	r->histvisib = room->histvisib;

	if (strrpl(&r->avatarurl, room->avatarurl))
		goto err_free_room;
	r->avatarinfo.w = room->avatarinfo.w;
	r->avatarinfo.h = room->avatarinfo.h;
	r->avatarinfo.size = room->avatarinfo.size;
	if (strrpl(&r->avatarinfo.mimetype, room->avatarinfo.mimetype))
		goto err_free_room;

	if (strrpl(&r->avatarthumburl, room->avatarthumburl))
		goto err_free_room;
	r->avatarthumbinfo.w = room->avatarthumbinfo.w;
	r->avatarthumbinfo.h = room->avatarthumbinfo.h;
	r->avatarthumbinfo.size = room->avatarthumbinfo.size;
	if (strrpl(&r->avatarthumbinfo.mimetype, room->avatarthumbinfo.mimetype))
		goto err_free_room;

	mtx_listentry_t *msgs = &r->msgs;
	mtx_list_dup(msgs, &room->msgs, mtx_msg_t, entry, dup_msg);
	if (!msgs)
		goto err_free_room;

	r->crypt.enabled = room->crypt.enabled;
	if (strrpl(&r->crypt.algorithm, room->crypt.algorithm))
		goto err_free_room;
	r->crypt.rotperiod = room->crypt.rotperiod;
	r->crypt.rotmsgnum = room->crypt.rotmsgnum;

	assert(room->history);
	mtx_room_history_t *history = dup_history(room->history);
	if (!history)
		goto err_free_room;
	r->history = history;
	r->context = r->context;

	r->dirty = r->dirty;

	return r;

err_free_room:
	mtx_free_room(r);
	return NULL;
}
void clear_room_direct_state(mtx_room_t *room)
{
	free(room->creator);
	room->creator = NULL;

	free(room->version);
	room->version = NULL;

	room->federate = 1;

	free(room->previd);
	room->previd = NULL;

	free(room->prev_last_eventid);
	room->prev_last_eventid = NULL;

	free(room->name);
	room->name = NULL;
	
	free(room->topic);
	room->topic = NULL;

	free(room->canonalias);
	room->canonalias = NULL;

	strarr_free(room->altaliases);
	room->altaliases = NULL;

	strarr_free(room->heroes);
	room->heroes = NULL;

	room->ninvited = 0;
	room->njoined = 0;
	room->notif_count = 0;
	room->notif_highlight_count = 0;

	mtx_list_free(&room->members, mtx_member_t, entry, free_member);
	mtx_list_init(&room->members);

	room->powerlevels.ban = 50;
	room->powerlevels.invite = 50;
	room->powerlevels.kick = 50;
	room->powerlevels.redact = 50;
	room->powerlevels.statedefault = 50;

	mtx_list_free(&room->powerlevels.events, mtx_event_powerlevel_t, entry, free_event_powerlevel);
	mtx_list_init(&room->powerlevels.events);
	room->powerlevels.eventdefault = 0;

	mtx_list_free(&room->powerlevels.users, mtx_user_powerlevel_t, entry, free_user_powerlevel);
	mtx_list_init(&room->powerlevels.users);
	room->powerlevels.usersdefault = 0;

	room->powerlevels.roomnotif = 50;

	room->joinrule = JOINRULE_NUM;

	room->histvisib = HISTVISIB_NUM;

	free(room->avatarurl);
	room->avatarurl = NULL;

	room->avatarinfo.w = 0;
	room->avatarinfo.h = 0;
	room->avatarinfo.size = 0;
	free(room->avatarinfo.mimetype);
	room->avatarinfo.mimetype = NULL;

	free(room->avatarthumburl);
	room->avatarthumburl = NULL;

	room->avatarthumbinfo.w = 0;
	room->avatarthumbinfo.h = 0;
	room->avatarthumbinfo.size = 0;
	free(room->avatarthumbinfo.mimetype);
	room->avatarthumbinfo.mimetype = NULL;

	mtx_list_free(&room->msgs, mtx_msg_t, entry, free_msg);
	mtx_list_init(&room->msgs);

	room->crypt.enabled = 0;
	free(room->crypt.algorithm);
	room->crypt.algorithm = NULL;
	room->crypt.rotperiod = 0;
	room->crypt.rotmsgnum = 0;

	room->dirty = 1;
}
