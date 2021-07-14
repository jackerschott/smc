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

int parse_command(const char *str, command_t *cmd)
{
	char *s = strdup(str);
	if (!s)
		return -1;


	size_t nargs = 0;
	char **args = malloc(ARGS_NUM_MAX * sizeof(*args));
	if (!args) {
		free(s);
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

	char *name = strdup(args[0]);
	if (!name) {
		free(args);
		free(s);
		return -1;
	}
	cmd->name = name;

	int err;
	if (strcmp(cmd->name, "invite") == 0) {
		if ((err = parse_invite(args + 1, nargs - 1, &cmd->invite)))
			return 1;
	} else {
		strcpy(cmd_last_err, "unknown command");
		return 1;
	}

	free(args);
	free(s);
	return 0;
}
