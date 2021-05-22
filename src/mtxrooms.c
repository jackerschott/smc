#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "api.h"
#include "smc.h"

enum operation_t {
	OPER_SHOW,
	OPER_CREATE,
	OPER_LEAVE,
	OPER_FORGET,
	OPER_JOIN,
	OPER_BAN,
};
typedef enum operation_t operation_t;

#define FLAG_LEAVE_FORGET 1

union {
	enum operation_t oper;
	struct {
		enum operation_t oper;
	} show;
	struct {
		enum operation_t oper;
		char *name;
		char *alias;
		char *topic;
		char *clientid;
		char *preset;
	} create;
	struct {
		enum operation_t oper;
		char *id;
		char *clientid;
		int flags;
	} leave;
	struct {
		enum operation_t oper;
		char *id;
		char *clientid;
	} forget;
	struct {
		enum operation_t oper;
		char *id;
		char *clientid;
	} join;
	struct {
		enum operation_t oper;
	} ban;
} options;

const char *token_file_path = "accesstoken";

#define HREAD_BUFSIZE 1024U
static int hread(int fd, char **buf, size_t *size)
{
	char *b = *buf;
	size_t sz = *size;
	while (1) {
		ssize_t n = read(fd, b, sz);
		if (n == -1) {
			free(buf);
			return -1;
		} else if (n == 0) {
			b[0] = '\0';
			return 0;
		}

		b += n;
		sz -= n;

		if (sz <= 0) {
			size_t newsize = *size + HREAD_BUFSIZE;
			char *newbuf = realloc(*buf, newsize);
			if (!newbuf) {
				free(buf);
				return -1;
			}
			b = newbuf + (b - *buf);
			sz += newsize - *size;
			*size = newsize;
			*buf = newbuf;
		}
	}
	assert(0);
}

int save_token(char *token)
{
	int ftoken = open(token_file_path, O_CREAT | O_WRONLY, 0600);
	if (ftoken == -1)
		return 1;

	ssize_t n = write(ftoken, token, strlen(token));
	if (n == -1) {
		close(ftoken);
		return 1;
	}

	close(ftoken);
	return 0;
}
int get_token(char **token)
{
	int ftoken = open(token_file_path, O_RDONLY);
	if (ftoken == -1) {
		if (errno == ENOENT)
			return 1;
		return -1;
	}

	size_t size = HREAD_BUFSIZE;
	char *buf = malloc(size);
	if (!buf) {
		close(ftoken);
		return -1;
	}

	if (hread(ftoken, &buf, &size) == -1)
		return -1;
	*token = buf;

	close(ftoken);
	return 0;
}

static void cleanup(void);

static void show(void)
{
	size_t nrooms;
	char **rooms;
	int err = api_room_list_joined(&rooms, &nrooms);
	if (err == -1) {
		fprintf(stderr, "%s: could not list joined rooms\n", __func__);
		cleanup();
		exit(1);
	} else if (err == 1) {
		fprintf(stderr, "%s: could not list joined rooms, %s (%i)\n",
				__func__, lasterrmsg, lastcode);
		cleanup();
		exit(1);
	}

	for (size_t i = 0; i < nrooms; ++i) {
		printf("%s\n", rooms[i]);
		free(rooms[i]);
	}
	free(rooms);
}
static void create(void)
{
	char *id;
	int err = api_room_create(options.create.clientid, options.create.name, options.create.alias,
			options.create.topic, options.create.preset, &id);
	if (err == -1) {
		fprintf(stderr, "%s: could not create room\n", __func__);
		cleanup();
		exit(1);
	} else if (err == 1) {
		fprintf(stderr, "%s: login failed, %s (%i)\n",
				__func__, lasterrmsg, lastcode);
		cleanup();
		exit(1);
	}
}
static void leave(void)
{
	int err = api_room_leave(options.leave.id);
	if (err == -1) {
		fprintf(stderr, "%s: error while leaving room\n", __func__);
		cleanup();
		exit(1);
	} else if (err == 1) {
		fprintf(stderr, "%s: could not leave room, %s (%i)\n",
				__func__, lasterrmsg, lastcode);
		cleanup();
		exit(1);
	}
	
	if (options.leave.flags & FLAG_LEAVE_FORGET) {
		err = api_room_forget(options.leave.id);
		if (err == -1) {
			fprintf(stderr, "%s: error while forgetting room\n", __func__);
			cleanup();
			exit(1);
		} else if (err == 1) {
			fprintf(stderr, "%s: could not forget room, %s (%i)\n",
					__func__, lasterrmsg, lastcode);
			cleanup();
			exit(1);
		}
	}
}
static void forget(void)
{
	int err = api_room_forget(options.forget.id);
	if (err == -1) {
		fprintf(stderr, "%s: error while forgetting room\n", __func__);
		cleanup();
		exit(1);
	} else if (err == 1) {
		fprintf(stderr, "%s: could not forget room, %s (%i)\n",
				__func__, lasterrmsg, lastcode);
		cleanup();
		exit(1);
	}
}
static void join(void)
{
	assert(0);
}

/* debug */
void print_options(void)
{
	switch (options.oper) {
		case OPER_SHOW:
			printf("[show]\n");
			break;
		case OPER_CREATE:
			printf("[create]\n");
			printf("name = %s\n", options.create.name);
			printf("alias = %s\n", options.create.alias);
			printf("topic = %s\n", options.create.topic);
			printf("clientid = %s\n", options.create.clientid);
			printf("preset = %s\n", options.create.preset);
			break;
		case OPER_LEAVE:
			printf("[leave]\n");
			printf("id = %s\n", options.leave.id);
			printf("clientid = %s\n", options.leave.clientid);
			break;
		case OPER_FORGET:
			printf("[forget]\n");
			printf("id = %s\n", options.forget.id);
			printf("clientid = %s\n", options.forget.clientid);
			break;
		case OPER_JOIN:
			printf("[join]\n");
			printf("id = %s\n", options.join.id);
			printf("clientid = %s\n", options.join.clientid);
			break;
		case OPER_BAN:
			assert(0);
	}
}
int parse_options_create(int argc, char *const argv[])
{
	int ret = 0;

	const char *optstr = ":Cn:a:t:i:p:";
	int c = getopt(argc, argv, optstr);
	assert(c == 'C');

	for (c = getopt(argc, argv, optstr); c != -1; c = getopt(argc, argv, optstr)) {
		if (c == ':') {
			printf("missing argument to '-%c'\n", optopt);
			ret = 1;
			goto err_cleanup_options;
		} else if (c == '?') {
			printf("unexpected option '-%c'\n", c);
			ret = 1;
			goto err_cleanup_options;
		}

		char *s = strdup(optarg);
		if (!s) {
			ret = -1;
			goto err_cleanup_options;
		}
		switch (c) {
		case 'n':
			options.create.name = s;
			break;
		case 'a':
			options.create.alias = s;
			break;
		case 't':
			options.create.topic = s;
			break;
		case 'i':
			options.create.clientid = s;
			break;
		case 'p':
			options.create.preset = s;
			break;
		default:
			assert(0);
		}
	}
	return 0;

err_cleanup_options:
	free(options.create.name);
	free(options.create.alias);
	free(options.create.topic);
	free(options.create.clientid);
	free(options.create.preset);
	return ret;
}
int parse_options_leave(int argc, char *const argv[])
{
	int ret;

	const char *optstr = ":Lr:i:f";
	int c = getopt(argc, argv, optstr);
	assert(c == 'L');

	for (c = getopt(argc, argv, optstr); c != -1; c = getopt(argc, argv, optstr)) { 
		if (c == ':') {
			printf("missing argument to '-%c'\n", optopt);
			ret = 1;
			goto err_cleanup_options;
		} else if (c == '?') {
			printf("unexpected option '-%c'\n", c);
			ret = 1;
			goto err_cleanup_options;
		}

		switch (c) {
		case 'f':
			options.leave.flags |= FLAG_LEAVE_FORGET;
			continue;
		}

		char *s = strdup(optarg);
		if (!s) {
			ret = -1;
			goto err_cleanup_options;
		}
		switch (c) {
		case 'r':
			options.leave.id = s;
			continue;
		case 'i':
			options.leave.clientid = s;
			continue;
		default:
			assert(0);
		}
	}

	if (!options.forget.id && !options.forget.clientid) {
		printf("one of -r or -i must be given\n");
		goto err_cleanup_options;
	}
	return 0;

err_cleanup_options:
	free(options.leave.id);
	free(options.leave.clientid);
	return ret;
}
int parse_options_forget(int argc, char *const argv[])
{
	int ret;

	const char *optstr = ":Fr:i:";
	int c = getopt(argc, argv, optstr);
	assert(c == 'F');

	for (c = getopt(argc, argv, optstr); c != -1; c = getopt(argc, argv, optstr)) {
		if (c == ':') {
			printf("missing argument to '-%c'\n", optopt);
			ret = 1;
			goto err_cleanup_options;
		} else if (c == '?') {
			printf("unexpected option '-%c'\n", c);
			ret = 1;
			goto err_cleanup_options;
		}

		char *s = strdup(optarg);
		if (!s) {
			ret = -1;
			goto err_cleanup_options;
		}
		switch (c) {
		case 'r':
			options.forget.id = s;
			break;
		case 'i':
			options.forget.clientid = s;
			break;
		default:
			assert(0);
		}
	}

	if (!options.forget.id && !options.forget.clientid) {
		printf("one of -r or -i must be given\n");
		goto err_cleanup_options;
	}
	return 0;

err_cleanup_options:
	free(options.forget.id);
	free(options.forget.clientid);
	return ret;
}
int parse_options_join(int argc, char *const argv[])
{
	int ret;

	const char *optstr = ":Jr:i:";
	int c = getopt(argc, argv, optstr);
	assert(c == 'J');

	for (c = getopt(argc, argv, optstr); c != -1; c = getopt(argc, argv, optstr)) { 
		if (c == ':') {
			printf("missing argument to '-%c'\n", optopt);
			ret = 1;
			goto err_cleanup_options;
		} else if (c == '?') {
			printf("unexpected option '-%c'\n", c);
			ret = 1;
			goto err_cleanup_options;
		}

		char *s = strdup(optarg);
		if (!s) {
			ret = -1;
			goto err_cleanup_options;
		}
		switch (c) {
		case 'r':
			options.join.id = s;
			break;
		case 'i':
			options.join.clientid = s;
			break;
		default:
			assert(0);
		}
	}

	if (!options.join.id && !options.join.clientid) {
		printf("one of -r or -i must be given\n");
		goto err_cleanup_options;
	}
	return 0;

err_cleanup_options:
	free(options.join.id);
	free(options.join.clientid);
	return ret;
}
int parse_options_ban(int c)
{
	assert(0);
}
int parse_options(int argc, char *const argv[])
{
	memset(&options, 0, sizeof(options));

	if (argc < 2) {
		printf("no operation specified\n");
		return 1;
	}

	if (strncmp(argv[1], "-S", STRLEN("-S")) == 0) {
		options.oper = OPER_SHOW;
		return 0;
	} else if (strncmp(argv[1], "-C", STRLEN("-C")) == 0) {
		options.oper = OPER_CREATE;
		return parse_options_create(argc, argv);
	} else if (strncmp(argv[1], "-L", STRLEN("-L")) == 0) {
		options.oper = OPER_LEAVE;
		return parse_options_leave(argc, argv);
	} else if (strncmp(argv[1], "-F", STRLEN("-F")) == 0) {
		options.oper = OPER_FORGET;
		return parse_options_forget(argc, argv);
	} else if (strncmp(argv[1], "-J", STRLEN("-J")) == 0) {
		options.oper = OPER_FORGET;
		return parse_options_join(argc, argv);
	} else if (strncmp(argv[1], "-B", STRLEN("-B")) == 0) {
		assert(0);
	}

	printf("unknown operation '%c%c'\n", argv[1][0], argv[1][1]);
	return 1;
}
int ensure_login(void)
{
	char *token = NULL;
	int err = get_token(&token);
	if (err == -1) {
		fprintf(stderr, "%s: could not check token file\n", __func__);
		return 1;
	} else if (err == 1) {
		err = api_login("alice", "alice", NULL, &token, NULL, NULL);
		if (err == -1) {
			fprintf(stderr, "%s: login failed\n", __func__);
			free(token);
			return 1;
		} else if (err == 1) {
			fprintf(stderr, "%s: login failed, %s (%i)\n",
					__func__, lasterrmsg, lastcode);
			free(token);
			return 1;
		}

		if (save_token(token)) {
			fprintf(stderr, "%s: could not save token\n", __func__);
			free(token);
			return 1;
		}
	}

	if (api_set_access_token(token)) {
		fprintf(stderr, "%s: could not set access token\n", __func__);
		free(token);
		return 1;
	}
	free(token);
	return 0;
}

void cleanup(void)
{
	api_cleanup();
}
int main(int argc, char *argv[])
{
	//int err = parse_options(argc, argv);
	//if (err == -1) {
	//	fprintf(stderr, "failed to parse options\n");
	//	return 1;
	//} else if (err == 1) {
	//	return 1;
	//}

	if (api_init())
		return 1;
	if (ensure_login()) {
		api_cleanup();
		return 1;
	}

	state_t *state;
	int err = api_sync(&state);
	if (err) {
		fprintf(stderr, "%s: could not sync\n", __func__);
		return 1;
	}
	printf("%s\n", state->next_batch);

	api_cleanup();
	return 0;

	switch (options.oper) {
	case OPER_SHOW:
		show();
		break;
	case OPER_CREATE:
		create();
		break;
	case OPER_LEAVE:
		leave();
		break;
	case OPER_FORGET:
		forget();
		break;
	case OPER_JOIN:
		join();
		break;
	case OPER_BAN:
		assert(0);
	}

	api_cleanup();
	return 0;
}
