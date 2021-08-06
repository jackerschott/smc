#include <assert.h>
#include <ctype.h> /* isspace */
#include <stdlib.h>
#include <string.h>

#include "msg/command.h"
#include "lib/list.h"

#define ARGS_NUM_MAX 128

char cmd_last_err[CMD_ERRORMSG_BUFSIZE];

int parse_invite(char **args, size_t nargs, command_invite_t *cmd)
{
	if (nargs == 0) {
		strcpy(cmd_last_err, "no argument supplied");
		return 1;
	}

	size_t nids = 0;
	char **ids = malloc(nargs * sizeof(*ids));
	if (!ids)
		return -1;

	for (size_t i = 0; i < nargs; ++i) {
		char *id = strdup(args[i]);
		if (!id)
			goto err_free;
		ids[i] = id;
		++nids;
	}

	cmd->nuserids = nids;
	cmd->userids = ids;
	return 0;

err_free:
	for (size_t i = 0; i < nids; ++i) {
		free(ids[i]);
	}
	free(ids);
	return -1;
}

int parse_command(const char *str, command_t **_cmd)
{
	command_t *cmd = malloc(sizeof(*cmd));
	if (!cmd)
		return -1;

	char *s = strdup(str);
	if (!s) {
		free(cmd);
		return -1;
	}


	size_t nargs = 0;
	char **args = malloc(ARGS_NUM_MAX * sizeof(*args));
	if (!args) {
		free(s);
		free(cmd);
		return -1;
	}

	int aftersep = 1;
	size_t n = strlen(s);
	for (size_t i = 0; i < n; ++i) {
		if (isspace(s[i])) {
			s[i] = '\0';
			aftersep = 1;
		} else if (aftersep) {
			args[nargs] = s + i;
			++nargs;

			aftersep = 0;

			assert(nargs < ARGS_NUM_MAX);
		}
	}
	if (nargs == 0) {
		free(args);
		free(s);
		free(cmd);
		return 2;
	}

	char *name = strdup(args[0]);
	if (!name) {
		free(args);
		free(s);
		free(cmd);
		return -1;
	}

	int err;
	if (strcmp(name, "invite") == 0) {
		if ((err = parse_invite(args + 1, nargs - 1, &cmd->invite))) {
			free(name);
			free(args);
			free(s);
			free(cmd);
			return err;
		}
	} else {
		strcpy(cmd_last_err, "unknown command");

		free(name);
		free(args);
		free(s);
		free(cmd);
		return 1;
	}
	free(args);
	free(s);

	cmd->name = name;
	*_cmd = cmd;
	return 0;
}

void free_cmd_invite(command_invite_t *cmd)
{
	for (size_t i = 0; i < cmd->nuserids; ++i) {
		free(cmd->userids[i]);
	}
}
void free_cmd(command_t *cmd)
{
	if (strcmp(cmd->name, "invite")) {
		free_cmd_invite(&cmd->invite);
	}
	free(cmd->name);
	free(cmd);
}
