#ifndef COMMAND_H
#define COMMAND_H

#include <stddef.h>

#define CMD_ERRORMSG_BUFSIZE 256
extern char cmd_last_err[CMD_ERRORMSG_BUFSIZE];

typedef struct {
	char *name;
	size_t nuserids;
	char **userids;
} command_invite_t;

typedef union {
	char *name;
	command_invite_t invite;
} command_t;

int parse_command(const char *str, command_t **_cmd);

void free_cmd(command_t *cmd);

#endif /* COMMAND_H */
