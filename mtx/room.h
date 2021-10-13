#ifndef MTX_ROOM_H
#define MTX_ROOM_H

#include <json-c/json.h>

#include "lib/list.h"

/*
   history types
*/
typedef enum {
	/* room events */
	/* state events */
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

	EVENT_GUEST_ACCESS,

	EVENT_SERVER_ACL,

	EVENT_TOMBSTONE,

	/* message events */
	EVENT_REDACTION,

	EVENT_MESSAGE,

	/* to device events */

	EVENT_NUM,
} mtx_eventtype_t;
static char *eventtype_strs[] = {
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

	"m.room.guest_access",

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
} mtx_ev_canonalias_t;

typedef struct {
	char *creator;
	int federate;
	char *version;

	char *previd;
	char *prev_last_eventid;
} mtx_ev_create_t;

typedef enum {
	JOINRULE_PUBLIC,
	JOINRULE_KNOCK,
	JOINRULE_INVITE,
	JOINRULE_PRIVATE,
	JOINRULE_NUM,
} mtx_joinrule_t;
typedef struct {
	mtx_joinrule_t rule;
} mtx_ev_joinrules_t;

typedef enum {
	MEMBERSHIP_INVITE,
	MEMBERSHIP_JOIN,
	MEMBERSHIP_KNOCK,
	MEMBERSHIP_LEAVE,
	MEMBERSHIP_BAN,
	MEMBERSHIP_NUM,
} mtx_membership_t;
typedef struct {
	char *displayname;
	char *mxid;
	json_object *signatures;
	char *token;
} mtx_thirdparty_invite_t;
typedef struct {
	char *avatarurl;
	char *displayname;
	mtx_membership_t membership;
	int isdirect;

	mtx_thirdparty_invite_t thirdparty_invite;
	mtx_listentry_t invite_room_events;
} mtx_ev_member_t;

typedef struct {
	mtx_listentry_t entry;
	mtx_eventtype_t type;
	int level;
} mtx_event_powerlevel_t;
typedef struct {
	mtx_listentry_t entry;
	char *id;
	int level;
} mtx_user_powerlevel_t;
typedef struct {
	int ban;
	int invite;
	int kick;
	int redact;
	int statedefault;

	mtx_listentry_t events;
	int eventdefault;

	mtx_listentry_t users;
	int usersdefault;

	int roomnotif;
} mtx_ev_powerlevels_t;

typedef struct {
	char *reason;
} mtx_ev_redaction_t;

/* instant messaging */
typedef struct {
	char *name;
} mtx_ev_name_t;

typedef struct {
	char *topic;
} mtx_ev_topic_t;

typedef struct {
	size_t h;
	size_t w;
	char *mimetype;
	size_t size;
} mtx_avatar_image_info_t;
typedef struct {
	char *url;
	mtx_avatar_image_info_t info;

	char *thumburl;
	// TODO: thumbfile;
	mtx_avatar_image_info_t thumbinfo;
} mtx_ev_avatar_t;

/* end-to-end encryption */
typedef struct {
	char *algorithm;
	unsigned long rotperiod;
	unsigned long rotmsgnum;
} mtx_ev_encryption_t;

typedef enum {
	MTX_CRYPT_ALGORITHM_OLM,
	MTX_CRYPT_ALGORITHM_MEGOLM,
	MTX_CRYPT_ALGORITHM_NUM,
} mtx_crypt_algorithm_t;
static char *mtx_crypt_algorithm_strs[] = {
	"m.olm.v1.curve25519-aes-sha2",
	"m.megolm.v1.aes-sha2",
};
typedef struct {
	mtx_listentry_t entry;

	char *identkey;

	char *body;
	int type;
} mtx_ciphertext_info_t;
typedef struct {
	struct {
		char *algorithm;
		char *senderkey;
	};
	struct {
		char *algorithm;
		char *senderkey;
		mtx_listentry_t ciphertext;
	} olm;
	struct {
		char *algorithm;
		char *senderkey;
		char *ciphertext;
		char *deviceid;
		char *sessionid;
	} megolm;
} mtx_ev_encrypted_t;

typedef struct {
	mtx_crypt_algorithm_t algorithm;
	char *roomid;
	char *sessionid;
	char *sessionkey;
} mtx_ev_room_key_t;

typedef enum {
	MTX_KEY_REQUEST,
	MTX_KEY_REQUEST_CANCEL,
	MTX_KEY_REQUEST_NUM,
} mtx_key_request_action_t;
static char *mtx_key_request_action_strs[] = {
	"request",
	"request_cancellation",
};
typedef struct {
	char *algorithm;
	char *roomid;
	char *senderkey;
	char *sessionid;
} mtx_requested_key_info_t;
typedef struct {
	mtx_requested_key_info_t body;
	mtx_key_request_action_t action;
	char *deviceid;
	char *requestid;
} mtx_ev_room_key_request_t;

/* room history visibility */
typedef enum {
	HISTVISIB_INVITED,
	HISTVISIB_JOINED,
	HISTVISIB_SHARED,
	HISTVISIB_WORLD,
	HISTVISIB_NUM,
} mtx_history_visibility_t;
static char *mtx_history_visibility_strs[] = {
	"invited",
	"joined",
	"shared",
	"world_readable",
};
typedef struct {
	mtx_history_visibility_t visib;
} mtx_ev_history_visibility_t;

/* guest access */
typedef enum {
	MTX_GUEST_ACCESS_CAN_JOIN,
	MTX_GUEST_ACCESS_FORBIDDEN,
	MTX_GUEST_ACCESS_NUM,
} mtx_guest_access_t;
static char *mtx_guest_access_strs[] = {
	"can_join",
	"forbidden",
};
typedef struct {
	mtx_guest_access_t access;
} mtx_ev_guest_access_t;

/* TODO: implement msg events */
typedef enum {
	MTX_MSG_TEXT,
	MTX_MSG_EMOTE,
	MTX_MSG_NOTICE,
	MTX_MSG_IMAGE,
	MTX_MSG_FILE,
	MTX_MSG_AUDIO,
	MTX_MSG_LOCATION,
	MTX_MSG_VIDEO,
	MTX_MSG_NUM,
} mtx_msg_type_t;
static char *mtx_msg_type_strs[] = {
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
} mtx_message_text_t;
typedef struct {
	char *fmt;
	char *fmtbody;
} mtx_message_emote_t;
typedef struct {
	mtx_msg_type_t type;
	char *body;
	void *content;
} mtx_ev_message_t;

// TODO: implement other message types

typedef struct mtx_event_t mtx_event_t;
struct mtx_event_t {
	mtx_listentry_t entry;

	mtx_eventtype_t type;
	char *id;
	char *sender;
	unsigned long ts; 

	unsigned long age;
	mtx_event_t *redactreason;
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
} mtx_room_summary_t;

typedef enum {
	// TODO: better naming
	EVENT_CHUNK_STATE, // state to fill gap
	EVENT_CHUNK_MESSAGE, // continous part with state and message events
} mtx_event_chunk_type_t;
typedef struct {
	mtx_listentry_t entry;
	
	mtx_event_chunk_type_t type;
	mtx_listentry_t events;
} mtx_event_chunk_t;
typedef struct {
	mtx_listentry_t chunks;
	int limited;
	char *prevbatch;
} mtx_timeline_t;

typedef struct {
	mtx_room_summary_t summary;
	mtx_timeline_t timeline;
	mtx_listentry_t ephemeral;
	mtx_listentry_t account;
	int notif_count;
	int notif_highlight_count;
} mtx_room_history_t;

typedef struct {
	char *algorithm;
	int count;
} mtx_otkey_count_t;

typedef struct {
	mtx_listentry_t changed;
	mtx_listentry_t left;
	mtx_listentry_t counts;
} mtx_device_info_t;

/*
   direct state types
*/
typedef struct {
	mtx_listentry_t entry;

	char *userid;
	mtx_membership_t membership;
	char *displayname;
	char *avatarurl;
	int isdirect;
} mtx_member_t;
typedef struct {
	int ban;
	int invite;
	int kick;
	int redact;
	int statedefault;

	mtx_listentry_t events;
	int eventdefault;

	mtx_listentry_t users;
	int usersdefault;

	int roomnotif;
} mtx_powerlevels_t;
typedef struct {
	char *id;
	char *lasteventid;
} mtx_prevroom_t;
typedef struct {
	int enabled;
	char *algorithm;
	int rotperiod;
	int rotmsgnum;
} mtx_encryption_t;

typedef struct {
	mtx_listentry_t entry;
	mtx_msg_type_t type;
	char *sender;
	char *body;
	void *content;
} mtx_msg_t;

typedef enum {
	MTX_ROOM_CONTEXT_JOIN,
	MTX_ROOM_CONTEXT_INVITE,
	MTX_ROOM_CONTEXT_LEAVE,
	MTX_ROOM_CONTEXT_NUM,
} mtx_room_context_t;
static char *mtx_room_visibility_strs[] = {
	"public",
	"private",
};
typedef enum {
	MTX_ROOM_VISIBILITY_PUBLIC,
	MTX_ROOM_VISIBILITY_PRIVATE,
	MTX_ROOM_VISIBILITY_NUM,
} mtx_room_visibility_t;

typedef struct {
	mtx_listentry_t entry;

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

	mtx_listentry_t members;
	mtx_powerlevels_t powerlevels;
	mtx_joinrule_t joinrule;
	mtx_history_visibility_t histvisib;

	char *avatarurl;
	mtx_avatar_image_info_t avatarinfo;
	char *avatarthumburl;
	mtx_avatar_image_info_t avatarthumbinfo;

	mtx_listentry_t msgs;

	mtx_encryption_t crypt;

	mtx_guest_access_t guestaccess;

	mtx_room_context_t context;
	mtx_room_visibility_t visib;
	mtx_room_history_t *history;

	int dirty;
} mtx_room_t;

mtx_member_t *mtx_find_member(mtx_listentry_t *members, const char *userid);

void mtx_free_room(mtx_room_t *r);
mtx_room_t *mtx_dup_room(mtx_room_t *room);

#endif /* MTX_ROOM_H */
