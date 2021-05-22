#ifndef API_H
#define API_H

#include "json-c/json.h"
#include "parse.h"

#define PARAM_BUFSIZE 2048U

extern int lastcode;
extern merror_t lasterr;
extern char lasterrmsg[ERRORMSG_BUFSIZE];

int api_init(void);
void api_cleanup(void);
int api_set_access_token(char *token);

int api_login(const char *username, const char *pass,
		char **id, char **token, char **homeserver, char **devid);
int api_room_create(const char *clientid, const char *name, const char *alias,
		const char *topic, const char *preset, char **id);
int api_room_leave(const char *id);
int api_room_forget(const char *id);
int api_room_list_joined(char ***joinedrooms, size_t *nrooms);
int api_sync(state_t **state);

#endif /* API_H */
