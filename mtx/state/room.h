#ifndef HISTORY_H
#define HISTORY_H

#include <json-c/json_types.h>

#include "lib/list.h"

/*
   history types
*/
typedef enum {
	/* room events */
	/* 	state events */
	EVENT_CANONALIAS,
	EVENT_CREATE,
	EVENT_JOINRULES,
	EVENT_MEMBER,
	EVENT_POWERLEVELS,

	EVENT_NAME,
	EVENT_TOPIC,
	EVENT_AVATAR,

	EVENT_ENCRYPTION,

	EVENT_HISTORY_VISIBILITY,

	EVENT_SERVER_ACL,

	EVENT_TOMBSTONE,

	/* 	message events */
	EVENT_REDACTION,

	EVENT_MESSAGE,

	EVENT_NUM,
} eventtype_t;
static const char *eventtype_strs[] = {
	/* room events */
	/* 	state events */
	"m.room.canonical_alias",
	"m.room.create",
	"m.room.join_rules",
	"m.room.member",
	"m.room.power_levels",

	"m.room.name",
	"m.room.topic",
	"m.room.avatar",

	"m.room.encryption",

	"m.room.history_visibility",

	"m.room.server_acl",

	"m.room.tombstone",

	/* 	message events */
	"m.room.redaction",
	"m.room.message",
};

/* base events */
typedef struct {
	char *alias;
	char **altaliases;
} ev_canonalias_t;

typedef struct {
	char *creator;
	int federate;
	char *version;

	char *previd;
	char *prev_last_eventid;
} ev_create_t;

typedef enum {
	JOINRULE_PUBLIC,
	JOINRULE_KNOCK,
	JOINRULE_INVITE,
	JOINRULE_PRIVATE,
	JOINRULE_NUM,
} joinrule_t;
static const char *joinrule_strs[] = {
	"public",
	"knock",
	"invite",
	"private",
};
typedef struct {
	joinrule_t rule;
} ev_joinrules_t;

typedef enum {
	MEMBERSHIP_INVITE,
	MEMBERSHIP_JOIN,
	MEMBERSHIP_KNOCK,
	MEMBERSHIP_LEAVE,
	MEMBERSHIP_BAN,
	MEMBERSHIP_NUM,
} membership_t;
static const char *membership_strs[] = {
	"invite",
	"join",
	"knock",
	"leave",
	"ban",
};
typedef struct {
	char *displayname;
	char *mxid;
	json_object *signatures;
	char *token;
} thirdparty_invite_t;
typedef struct {
	char *avatarurl;
	char *displayname;
	membership_t membership;
	int isdirect;

	thirdparty_invite_t thirdparty_invite;
	listentry_t invite_room_events;
} ev_member_t;

typedef struct {
	listentry_t entry;
	eventtype_t type;
	int level;
} event_powerlevel_t;
typedef struct {
	listentry_t entry;
	char *id;
	int level;
} user_powerlevel_t;
typedef struct {
	int ban;
	int invite;
	int kick;
	int redact;
	int statedefault;

	listentry_t events;
	int eventdefault;

	listentry_t users;
	int usersdefault;

	int roomnotif;
} ev_powerlevels_t;

typedef struct {
	char *reason;
} ev_redaction_t;

/* instant messaging */
typedef struct {
	char *name;
} ev_name_t;

typedef struct {
	char *topic;
} ev_topic_t;

typedef struct {
	size_t h;
	size_t w;
	char *mimetype;
	size_t size;
} avatar_image_info_t;
typedef struct {
	char *url;
	avatar_image_info_t info;

	char *thumburl;
	// TODO: thumbfile;
	avatar_image_info_t thumbinfo;
} ev_avatar_t;

/* end-to-end encryption */
typedef struct {
	char *algorithm;
	unsigned long rotperiod;
	unsigned long rotmsgnum;
} ev_encryption_t;

/* room history visibility */
typedef enum {
	HISTVISIB_INVITED,
	HISTVISIB_JOINED,
	HISTVISIB_SHARED,
	HISTVISIB_WORLD,
	HISTVISIB_NUM,
} history_visibility_t;
static const char *history_visibility_strs[] = {
	"invited",
	"joined",
	"shared",
	"world_readable",
};
typedef struct {
	history_visibility_t visib;
} ev_history_visibility_t;

/* TODO: implement msg events */
typedef enum {
	MSG_TEXT,
	MSG_EMOTE,
	MSG_NOTICE,
	MSG_IMAGE,
	MSG_FILE,
	MSG_AUDIO,
	MSG_LOCATION,
	MSG_VIDEO,
	MSG_NUM,
} msg_type_t;
static const char *msg_type_strs[] = {
	"m.text",
	"m.emote",
	"m.notice",
	"m.image",
	"m.file",
	"m.audio",
	"m.location",
	"m.video",
};
typedef struct {
	char *fmt;
	char *fmtbody;
} message_text_t;
typedef struct {
	char *fmt;
	char *fmtbody;
} message_emote_t;
typedef struct {
	msg_type_t type;
	char *body;
	void *content;
} ev_message_t;

// TODO: implement other message types

typedef struct event_t event_t;
struct event_t {
	listentry_t entry;

	eventtype_t type;
	char *id;
	char *sender;
	unsigned long ts; 

	unsigned long age;
	event_t *redactreason;
	char *txnid;

	char *roomid;

	char *statekey;

	char *redacts;

	void *prevcontent;
	void *content;
};

typedef struct {
	char **heroes;
	int njoined;
	int ninvited;
} room_summary_t;

typedef enum {
	// TODO: better naming
	EVENT_CHUNK_STATE, // state to fill gap
	EVENT_CHUNK_MESSAGE, // continous part with state and message events
} event_chunk_type_t;
typedef struct {
	listentry_t entry;
	
	event_chunk_type_t type;
	listentry_t events;
} event_chunk_t;
typedef struct {
	listentry_t chunks;
	int limited;
	char *prevbatch;
} timeline_t;

typedef struct {
	room_summary_t summary;
	timeline_t timeline;
	listentry_t ephemeral;
	listentry_t account;
	int notif_count;
	int notif_highlight_count;
} room_history_t;

typedef struct {
	char *algorithm;
	int count;
} otkey_count_t;

typedef struct {
	listentry_t changed;
	listentry_t left;
	listentry_t counts;
} device_info_t;

/*
   direct state types
*/
typedef struct {
	listentry_t entry;

	char *userid;
	membership_t membership;
	char *displayname;
	char *avatarurl;
	int isdirect;
} member_t;
typedef struct {
	int ban;
	int invite;
	int kick;
	int redact;
	int statedefault;

	listentry_t events;
	int eventdefault;

	listentry_t users;
	int usersdefault;

	int roomnotif;
} powerlevels_t;
typedef struct {
	char *id;
	char *lasteventid;
} prevroom_t;
typedef struct {
	int enabled;
	char *algorithm;
	int rotperiod;
	int rotmsgnum;
} encryption_t;

typedef struct {
	listentry_t entry;
	msg_type_t type;
	char *sender;
	char *body;
	void *content;
} msg_t;

typedef enum {
	ROOM_CONTEXT_JOIN,
	ROOM_CONTEXT_INVITE,
	ROOM_CONTEXT_LEAVE,
} room_context_t;

typedef struct _room_t _room_t;
struct _room_t {
	listentry_t entry;

	char *id;

	char *creator;
	char *version;
	int federate;
	char *previd;
	char *prev_last_eventid;

	char *name;
	char *topic;
	char *canonalias;
	char **altaliases;

	char **heroes;
	int ninvited;
	int njoined;
	int notif_count;
	int notif_highlight_count;

	listentry_t members;
	powerlevels_t powerlevels;
	joinrule_t joinrule;
	history_visibility_t histvisib;

	char *avatarurl;
	avatar_image_info_t avatarinfo;
	char *avatarthumburl;
	avatar_image_info_t avatarthumbinfo;

	listentry_t msgs;

	encryption_t crypt;

	room_context_t context;
	room_history_t *history;

	int dirty;
};

/* history */
void free_ev_canonalias(ev_canonalias_t *canonalias);

void free_ev_create(ev_create_t *create);

void free_ev_joinrules(ev_joinrules_t *joinrules);

void free_ev_member(ev_member_t *member);

void free_event_powerlevel(event_powerlevel_t *plevel);
void free_user_powerlevel(user_powerlevel_t *plevel);
void free_ev_powerlevels(ev_powerlevels_t *powerlevels);
event_powerlevel_t *find_event_powerlevel(const listentry_t *plevels, eventtype_t type);
user_powerlevel_t *find_user_powerlevel(const listentry_t *plevels, const char *id);

void free_ev_redaction(ev_redaction_t *redaction);

void free_ev_name(ev_name_t *name);

void free_ev_avatar(ev_avatar_t *avatar);

void free_ev_encryption(ev_encryption_t *encryption);

void free_ev_history_visibility(ev_history_visibility_t *visib);

void free_message_text(message_text_t *msg);
void free_message_emote(message_emote_t *msg);
void free_message_content(msg_type_t type, void *content);
void free_ev_message(ev_message_t *msg);
void *dup_message_content(msg_type_t type, void *content);

void free_event_content(eventtype_t type, void *content);
void free_event(event_t *event);
void free_event_chunk(event_chunk_t *chunk);
int is_roomevent(eventtype_t type);
int is_statevent(eventtype_t type);
int is_message_event(eventtype_t type);
event_t *find_event(listentry_t *chunks, const char *eventid);

void free_room_history(room_history_t *history);
void free_room_history_context(_room_t *r);
_room_t *find_room(listentry_t *rooms, const char *id);

/* direct state */
void free_member(member_t *m);
member_t *new_member(void);
member_t *find_member(listentry_t *members, const char *userid);

void free_room_direct_state_context(_room_t *r);

/* general */
void free_room(_room_t *r);
_room_t *new_room(const char *id, room_context_t context);
_room_t *dup_room(_room_t *room);
void clear_room_direct_state(_room_t *r);

#endif /* HISTORY_H */
