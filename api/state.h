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

	char *name;
	membership_t membership;
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

int apply_state_updates(json_object *obj, listentry_t *joinedrooms,
		listentry_t *invitedrooms, listentry_t *leftrooms);

#endif /* STATE_H */
