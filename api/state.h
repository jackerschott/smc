#ifndef STATE_H
#define STATE_H

#include <stdint.h>
#include <json-c/json_types.h>

#include "list.h"

typedef enum {
	MEMBERSHIP_INVITE,
	MEMBERSHIP_JOIN,
	MEMBERSHIP_LEAVE,
	MEMBERSHIP_BAN,
	MEMBERSHIP_KNOCK,
	MEMBERSHIP_NUM,
} membership_t;
typedef enum {
	JOINRULE_PUBLIC,
	JOINRULE_KNOCK,
	JOINRULE_INVITE,
	JOINRULE_PRIVATE,
	JOINRULE_NUM,
} joinrule_t;
typedef enum {
	HISTVISIB_INVITED,
	HISTVISIB_JOINED,
	HISTVISIB_SHARED,
	HISTVISIB_WORLD,
	HISTVISIB_NUM,
} history_visibility_t;
typedef struct {
	listentry_t entry;

	char *userid;
	membership_t membership;
	char *displayname;
	char *avatarurl;
} member_t;
typedef struct {
	listentry_t entry;

	char *name;
	int level;
} powerlevel_t;
typedef struct {
	int invite;
	int kick;
	int ban;
	int redact;

	int statedefault;
	int eventsdefault;
	listentry_t events;
	int usersdefault;
	listentry_t users;

	struct {
		int room;
	} notif;
} powerlevels_t;
typedef struct {
	char *id;
	char *lasteventid;
} prevroom_t;

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

typedef struct {
	listentry_t entry;
	msg_type_t type;
	char *sender;
	char *body;
} msg_t;
typedef struct {
	msg_t msg;
	char *format;
	char *fmtbody;
} msg_text_t;

typedef struct {
	listentry_t entry;

	char *id;
	char *name;
	char *topic;
	listentry_t members;
	powerlevels_t powerlevels;
	joinrule_t joinrule;
	history_visibility_t histvisib;
	char *creator;
	char *version;
	int federate;
	prevroom_t *replacetarget;

	listentry_t messages;
} room_t;

static const char *history_visibility_str[] = {
	"invited",
	"joined",
	"shared",
	"world_readable",
};
static const char *joinrule_str[] = {
	"public",
	"knock",
	"invite",
	"private",
};
static const char *membership_str[] = {
	"invite",
	"join",
	"leave",
	"ban",
	"knock"
};

static const char *msg_type_str[] = {
	"m.text",
	"m.emote",
	"m.notice",
	"m.image",
	"m.file",
	"m.audio",
	"m.location",
	"m.video",
};

void free_room(room_t *room);

int apply_sync_state_updates(json_object *obj, listentry_t *joinedrooms,
		listentry_t *invitedrooms, listentry_t *leftrooms);

#endif /* STATE_H */
