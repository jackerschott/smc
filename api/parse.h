#ifndef PARSE_H
#define PARSE_H

#include <json-c/json.h>
#include <stdint.h>

#include "list.h"

/* event types */

typedef enum {
	/* roomevents */
	/* state events */
	M_ROOM_CREATE,
	M_ROOM_MEMBER,
	M_ROOM_JOINRULES,
	M_ROOM_POWERLEVELS,
	M_ROOM_CANONICAL_ALIAS,

	M_ROOM_NAME,
	M_ROOM_TOPIC,

	M_ROOM_HISTORY_VISIBILITY,

	/* message events */
	M_ROOM_REDACTION,


	EVENT_TYPE_NUM,
} event_type_t;
static const char *event_type_strs[] = {
	"m.room.create",
	"m.room.member",
	"m.room.join_rules",
	"m.room.power_levels",
	"m.room.canonical_alias",

	"m.room.name",
	"m.room.topic",

	"m.room.history_visibility",

	"m.room.redaction",
};
typedef struct {
	listentry_t entry;
	event_type_t type;
} event_t;
typedef struct {
	event_t event;

	char *id;
	char *sender;
	long ts;
	long age;
	event_t *redactreason;
	char *transactid;
} roomevent_t;
typedef struct {
	roomevent_t revent;

	char *statekey;
	event_t *prevcontent;
} statevent_t;

typedef enum {
	JOINRULES_PUBLIC,
	JOINRULES_KNOCK,
	JOINRULES_INVITE,
	JOINRULES_PRIVATE,
	JOINRULES_NUM,
} joinrules_t;
static const char *joinrules_str[] = {
	"public",
	"knock",
	"invite",
	"private",
};
typedef enum {
	MEMBERSHIP_INVITE,
	MEMBERSHIP_JOIN,
	MEMBERSHIP_LEAVE,
	MEMBERSHIP_BAN,
	MEMBERSHIP_KNOCK,
	MEMBERSHIP_NUM,
} membership_t;
static const char *membership_str[] = {
	"invite",
	"join",
	"leave",
	"ban",
	"knock"
};
typedef struct {
	statevent_t sevent;

	char *creator;
	int federate;
	char *roomversion;
	struct {
		char *roomid;
		char *eventid;
	} prevroom;
} roomevent_create_t;
typedef struct {
	statevent_t sevent;

	char *avatarurl;
	char *displayname;
	membership_t membership;
	int isdirect;
	struct {
		char *displayname;
	} thirdparty_invite;
	struct {
		listentry_t *events;
	} invite_room_state;
} roomevent_member_t;
typedef struct {
	statevent_t sevent;

	joinrules_t rule;
} roomevent_joinrules_t;
typedef struct {
	statevent_t sevent;

	int invite;
	int kick;
	int ban;
	int redact;
	int statedefault;
	struct {
		size_t num;
		char **names;
		int *levels;
	} events;
	int eventsdefault;
	struct {
		size_t num;
		char **names;
		int *levels;
	} users;
	int usersdefault;
	struct {
		int room;
	} notifications;
} roomevent_powerlevels_t;
typedef struct {
	statevent_t sevent;

	char *alias;
	size_t naltaliases;
	char **altaliases;
} roomevent_canonical_alias_t;
typedef struct {
	roomevent_t revent;

	char *reason;
} roomevent_redaction_t;

typedef enum {
	MSGTYPE_TEXT,
	MSGTYPE_EMOTE,
	MSGTYPE_NOTICE,
	MSGTYPE_IMAGE,
} msgtype_t;
static const char *msgtype_strs[] = {
	"m.text",
	"m.emote",
	"m.notice",
	"m.image",
};
typedef struct {
	char *body;
	msgtype_t msgtype;
} roomevent_message_t;
typedef struct {
	roomevent_message_t msg;

	char *format;
	char *formbody;
} roomevent_message_text_t;
typedef struct {
	roomevent_message_t msg;

	char *format;
	char *formbody;
} roomevent_message_emote_t;
typedef struct {
	roomevent_message_t msg;

	char *format;
	char *formbody;
} roomevent_message_notice_t;
typedef struct {
	roomevent_message_t msg;

	struct {
		unsigned int h;
		unsigned int w;
		char *mimetype;
		size_t size;
		char *thumburl;
		/*encrypted_file_t *thumbfile*/
	} info;
	char *url;
	/*encrypted_file_t *file*/
} roomevent_message_image_t;

struct roomevent_message_feedback_t {

};
typedef struct {
	statevent_t sevent;

	char *name;
} roomevent_name_t;
typedef struct {
	statevent_t sevent;
	
	char *topic;
} roomevent_topic_t;
struct roomevent_avatar_t {

};
struct roomevent_pinned_events_t {

};

struct roomevent_call_invite_t {

};
struct roomevent_call_candidates_t {

};
struct roomevent_call_answer_t {

};
struct roomevent_call_hangup_t {

};

struct roomevent_typing_t {

};
struct roomevent_receipt_t {

};
struct roomevent_fullyread_t {

};
struct roomevent_presence_t {

};

/* encryption */
struct roomevent_key_verify_request_t {

};
struct roomevent_key_verify_start_t {

};
struct roomevent_key_verify_cancel_t {

};
struct roomevent_key_verify_accept_t {

};
struct roomevent_key_verify_key_t {

};
struct roomevent_key_verify_mac_t {

};

struct roomevent_olm_v1_curve25519_aes_sha2_t {

};
struct roomevent_megolm_v1_aes_sha2_t {

};

struct roomevent_encryption_t {

};
struct roomevent_encrypted_t {

};
struct roomevent_roomkey_t {

};
struct roomevent_roomkey_request_t {

};
struct roomevent_forwarded_roomkey_t {

};
struct roomevent_dummy_t {

};

typedef enum {
	HISTVISIB_INVITED,
	HISTVISIB_JOINED,
	HISTVISIB_SHARED,
	HISTVISIB_WORLD,
	HISTVISIB_NUM,
} history_visibility_t;
static const char *history_visibility_str[] = {
	"invited",
	"joined",
	"shared",
	"world_readable",
};
typedef struct {
	statevent_t sevent;
	history_visibility_t visib;
} roomevent_history_visibility_t;

/* roominfo types */
typedef enum {
	ROOMINFO_JOINED,
	ROOMINFO_INVITED,
	ROOMINFO_LEFT,
	ROOMINFO_NUM
} roominfo_type_t;
typedef struct {
	size_t nheroes;
	char **heroes;
	unsigned int njoined;
	unsigned int ninvited;
} roomsummary_t;
typedef struct {
	listentry_t *events;
} roomstate_t;
typedef struct {
	listentry_t *events;
	int limited;
	char *prevbatch;
} timeline_t;
typedef struct {
	listentry_t *events;
} epheremal_t;
typedef struct {
	listentry_t *events;
} accountstate_t;

typedef struct {
	listentry_t entry;
	roominfo_type_t type;
	char *id;
} roominfo_t;
typedef struct {
	roominfo_t rinfo;

	roomsummary_t summary;
	roomstate_t roomstate;
	timeline_t timeline;
	epheremal_t ephstate;
	accountstate_t accountstate;
	unsigned int hlnotenum;
	unsigned int notenum;
} roominfo_joined_t;
typedef struct {
	roominfo_t rinfo;

	roomstate_t state;
} roominfo_invited_t;
typedef struct {
	roominfo_t rinfo;

	roomstate_t state;
	timeline_t timeline;
	accountstate_t accountdata;
} roominfo_left_t;

typedef struct {
	char *next_batch;
	listentry_t *rooms;
} state_t;

#define MERROR_BUFSIZE 40U
#define ERRORMSG_BUFSIZE 256U
typedef enum {
	M_SUCCESS, /* not part of the spec */
	M_FORBIDDEN,
	M_UNKNOWN_TOKEN, \
	M_MISSING_TOKEN,
	M_BAD_JSON,
	M_NOT_JSON,
	M_NOT_FOUND,
	M_LIMIT_EXCEEDED,
	M_UNKNOWN,
	M_UNRECOGNIZED,
	M_UNAUTHORIZED,
	M_USER_DEACTIVATED,
	M_USER_IN_USE,
	M_INVALID_USERNAME,
	M_ROOM_IN_USE,
	M_INVALID_ROOM_STATE,
	M_THREEPID_IN_USE,
	M_THREEPID_NOT_FOUND,
	M_THREEPID_AUTH_FAILED,
	M_THREEPID_DENIED,
	M_SERVER_NOT_TRUSTED,
	M_UNSUPPORTED_ROOM_VERSION,
	M_INCOMPATIBLE_ROOM_VERSION,
	M_BAD_STATE,
	M_GUEST_ACCESS_FORBIDDEN,
	M_CAPTCHA_NEEDED,
	M_CAPTCHA_INVALID,
	M_MISSING_PARAM,
	M_INVALID_PARAM,
	M_TOO_LARGE,
	M_EXCLUSIVE,
	M_RESOURCE_LIMIT_EXCEEDED,
	M_CANNOT_LEAVE_SERVER_NOTICE_ROOM,
	MERROR_NUM,
} merror_t;
static const char *merrorstr[] = {
	"M_SUCCESS",
	"M_FORBIDDEN",
	"M_UNKNOWN_TOKEN",
	"M_MISSING_TOKEN",
	"M_BAD_JSON",
	"M_NOT_JSON",
	"M_NOT_FOUND",
	"M_LIMIT_EXCEEDED",
	"M_UNKNOWN",
	"M_UNRECOGNIZED",
	"M_UNAUTHORIZED",
	"M_USER_DEACTIVATED",
	"M_USER_IN_USE",
	"M_INVALID_USERNAME",
	"M_ROOM_IN_USE",
	"M_INVALID_ROOM_STATE",
	"M_THREEPID_IN_USE",
	"M_THREEPID_NOT_FOUND",
	"M_THREEPID_AUTH_FAILED",
	"M_THREEPID_DENIED",
	"M_SERVER_NOT_TRUSTED",
	"M_UNSUPPORTED_ROOM_VERSION",
	"M_INCOMPATIBLE_ROOM_VERSION",
	"M_BAD_STATE",
	"M_GUEST_ACCESS_FORBIDDEN",
	"M_CAPTCHA_NEEDED",
	"M_CAPTCHA_INVALID",
	"M_MISSING_PARAM",
	"M_INVALID_PARAM",
	"M_TOO_LARGE",
	"M_EXCLUSIVE",
	"M_RESOURCE_LIMIT_EXCEEDED",
	"M_CANNOT_LEAVE_SERVER_NOTICE_ROOM",
};

int get_response_error(const json_object *resp, int code, merror_t *error, char *errormsg);

void free_state(state_t *state);
int parse_state(json_object *obj, state_t **state);

#endif /* PARSE_H */
