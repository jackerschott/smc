#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "mtx/mtx.h"
#include "tests/utils.h"

int testerr;

typedef enum {
	TEST_RUNTYPE_INIT_SERVER = 1,
	TEST_RUNTYPE_RUN_TEST = 2,
	TEST_RUNTYPE_CLEANUP = 4,
} runflags_t;
typedef struct {
	runflags_t runtype;
} test_args_t;

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
int save_config(const char *filepath, const char *val)
{
	int ftoken = open(filepath, O_CREAT | O_WRONLY | O_TRUNC, 0600);
	if (ftoken == -1)
		return 1;

	ssize_t n = write(ftoken, val, strlen(val));
	if (n == -1) {
		close(ftoken);
		return 1;
	}

	close(ftoken);
	return 0;
}
int get_config(const char *filepath, char **val)
{
	int ftoken = open(filepath, O_RDONLY);
	if (ftoken == -1) {
		if (errno == ENOENT) {
			*val = NULL;
			return 0;
		}
		return 1;
	}

	size_t size = HREAD_BUFSIZE;
	char *buf = malloc(size);
	if (!buf) {
		close(ftoken);
		return 1;
	}

	if (hread(ftoken, &buf, &size) == -1)
		return 1;
	*val = buf;

	close(ftoken);
	return 0;
}

int change_login(mtx_session_t *session, char *username, int logout)
{
	if (logout) {
		if (mtx_logout(session))
			return 1;
	}

	mtx_id_t *id = mtx_create_id_user(username);
	if (mtx_login_password(session, "10.89.64.2:8008", id, username, NULL, NULL))
		return 1;
	mtx_free_id(id);

	return 0;
}

#define PRE_SERVER_INIT_FPATH (SCRIPT_DIR "pre_server_init.sh")
#define POST_SERVER_INIT_FPATH (SCRIPT_DIR "post_server_init.sh")
#define PRE_TEST_FPATH (SCRIPT_DIR "pre_test.sh")
#define POST_TEST_FPATH (SCRIPT_DIR "post_test.sh")
#define SERVER_CLEANUP_FPATH (SCRIPT_DIR "cleanup_server.sh")

test_args_t args;

static int parse_args(int argc, char **argv, test_args_t *args)
{
	if (argc > 2)
		return 1;

	if (argc == 1) {
		args->runtype = TEST_RUNTYPE_INIT_SERVER | TEST_RUNTYPE_RUN_TEST | TEST_RUNTYPE_CLEANUP;
	} else if (argc == 2 && strcmp(argv[1], "init") == 0) {
		args->runtype = TEST_RUNTYPE_INIT_SERVER;
	} else if (argc == 2 && strcmp(argv[1], "run") == 0) {
		args->runtype = TEST_RUNTYPE_RUN_TEST;
	} else if (argc == 2 && strcmp(argv[1], "cleanup") == 0) {
		args->runtype = TEST_RUNTYPE_CLEANUP;
	} else {
		return 1;
	}

	return 0;
}
static int run_script(char *filename)
{
	assert(filename);

	int err = system(filename);
	if (err == -1) {
		return 1;
	} else if (err != 0) {
		return 1;
	}

	return 0;
}
static void cleanup()
{
	if (args.runtype & TEST_RUNTYPE_CLEANUP)
		run_script(SERVER_CLEANUP_FPATH);
}
int run(int argc, char **argv, int (*init_server)(void), int (*test)(void))
{
	if (parse_args(argc, argv, &args)) {
		fprintf(stderr, "err: invalid arguments\n");
		return 1;
	}

	if (access(PRE_SERVER_INIT_FPATH, F_OK) != 0) {
		fprintf(stderr, "err: file `%s' does not exit\n", PRE_SERVER_INIT_FPATH);
		return 1;
	}
	if (access(POST_SERVER_INIT_FPATH, F_OK) != 0) {
		fprintf(stderr, "err: file `%s' does not exit\n", POST_SERVER_INIT_FPATH);
		return 1;
	}
	if (access(PRE_TEST_FPATH, F_OK) != 0) {
		fprintf(stderr, "err: file `%s' does not exit\n", PRE_TEST_FPATH);
		return 1;
	}
	if (access(POST_TEST_FPATH, F_OK) != 0) {
		fprintf(stderr, "err: file `%s' does not exit\n", POST_TEST_FPATH);
		return 1;
	}
	if (access(SERVER_CLEANUP_FPATH, F_OK) != 0) {
		fprintf(stderr, "err: file `%s' does not exit\n", SERVER_CLEANUP_FPATH);
		return 1;
	}

	struct stat st = {0};
	if (stat(TEST_CONFIG_DIR, &st) == -1 && mkdir(TEST_CONFIG_DIR, 0755) == -1) {
		fprintf(stderr, "err: could not create `%s'\n", TEST_CONFIG_DIR);
		return 1;
	}

	if (args.runtype & TEST_RUNTYPE_INIT_SERVER) {
		if (run_script(PRE_SERVER_INIT_FPATH))
			goto err_cleanup;
		sleep(4);

		if (init_server())
			goto err_cleanup;

		if (run_script(POST_SERVER_INIT_FPATH))
			goto err_cleanup;
	}

	if (args.runtype & TEST_RUNTYPE_RUN_TEST) {
		if (run_script(PRE_TEST_FPATH))
			goto err_cleanup;
		sleep(4);

		if (test())
			goto err_cleanup;

		if (run_script(POST_TEST_FPATH))
			goto err_cleanup;
	}

	cleanup();
	return 0;

err_cleanup:
	cleanup();
	return 1;
}
