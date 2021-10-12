#ifndef MTX_STATE_ROOM_H
#define MTX_STATE_ROOM_H

#include <json-c/json_types.h>

#include "lib/list.h"
#include "mtx/room.h"

static char *joinrule_strs[] = {
	"public",
	"knock",
	"invite",
	"private",
};
static char *mtx_membership_strs[] = {
	"invite",
	"join",
	"knock",
	"leave",
	"ban",
};

/* history */
void free_ev_canonalias(mtx_ev_canonalias_t *canonalias);

void free_ev_create(mtx_ev_create_t *create);

void free_ev_joinrules(mtx_ev_joinrules_t *joinrules);

void free_ev_member(mtx_ev_member_t *member);

void free_event_powerlevel(mtx_event_powerlevel_t *plevel);
void free_user_powerlevel(mtx_user_powerlevel_t *plevel);
void free_ev_powerlevels(mtx_ev_powerlevels_t *powerlevels);
mtx_event_powerlevel_t *find_event_powerlevel(const mtx_listentry_t *plevels, mtx_eventtype_t type);
mtx_user_powerlevel_t *find_user_powerlevel(const mtx_listentry_t *plevels, const char *id);

void free_ev_redaction(mtx_ev_redaction_t *redaction);

void free_ev_name(mtx_ev_name_t *name);

void free_ev_avatar(mtx_ev_avatar_t *avatar);

void free_ev_encryption(mtx_ev_encryption_t *encryption);

void free_ciphertext_info(mtx_ciphertext_info_t *info);
void free_ev_encrypted(mtx_ev_encrypted_t *encrypted);
json_object *format_ev_encrypted(const mtx_ev_encrypted_t *encrypted);

void free_ev_room_key_request(mtx_ev_room_key_request_t *request);
json_object *format_ev_room_key_request(const mtx_ev_room_key_request_t *keyrequest);

void free_ev_history_visibility(mtx_ev_history_visibility_t *visib);

void free_ev_guest_access(mtx_ev_guest_access_t *guestaccess);

void free_message_text(mtx_message_text_t *msg);
void free_message_emote(mtx_message_emote_t *msg);
void free_message_content(mtx_msg_type_t type, void *content);
void *dup_message_content(mtx_msg_type_t type, void *content);
void free_ev_message(mtx_ev_message_t *msg);
json_object *format_ev_message(const mtx_ev_message_t *msg);

void free_event_content(mtx_eventtype_t type, void *content);
void free_event(mtx_event_t *event);
void free_event_chunk(mtx_event_chunk_t *chunk);
int is_roomevent(mtx_eventtype_t type);
int is_statevent(mtx_eventtype_t type);
int is_message_event(mtx_eventtype_t type);
mtx_event_t *find_event(mtx_listentry_t *chunks, const char *eventid);

void free_room_history(mtx_room_history_t *history);
void free_room_history_context(mtx_room_t *r);
mtx_room_t *find_room(mtx_listentry_t *rooms, const char *id);

/* direct state */
void free_member(mtx_member_t *m);
mtx_member_t *new_member(void);

void free_msg(mtx_msg_t *m);

void free_room_direct_state_context(mtx_room_t *r);

/* general */
void free_room(mtx_room_t *r);
mtx_room_t *new_room(const char *id, mtx_room_context_t context);
mtx_room_t *dup_room(mtx_room_t *room);
void clear_room_direct_state(mtx_room_t *r);

#endif /* MTX_STATE_ROOM_H */
