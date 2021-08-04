#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ncurses.h>

#include "api/api.h"
#include "api/state.h"
#include "lib/list.h"
#include "msg/menu.h"
#include "msg/smc.h"
#include "msg/sync.h"
#include "msg/room.h"
#include "msg/ui.h"

#define SYNC_HANDLER_STACKSIZE (4096 * 64)

pthread_t sync_handler;
int smc_terminate = 0;

int flog;

struct {
	char *username;
	char *pass;
	char *name;
} options;

#define TOKEN_FILE_PATH (CONFIG_DIR "accesstoken")
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
static int save_token(char *token)
{
	int ftoken = open(TOKEN_FILE_PATH, O_CREAT | O_WRONLY | O_TRUNC, 0600);
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
static int get_token(char **token)
{
	int ftoken = open(TOKEN_FILE_PATH, O_RDONLY);
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

static int ensure_login(void)
{
	int err;
	char *token = NULL;
	if (!options.username || !options.pass) {
		err = get_token(&token);
		if (err == -1) {
			fprintf(stderr, "%s: could not check token file\n", __func__);
			return 1;
		} else if (err == 1) {
			printf("unable to login: no username or password supplied\n");
			free(token);
			return 1;
		}
	} else if (options.username && options.pass) {
		err = api_login(options.username, options.pass, NULL, &token, NULL, NULL);
		if (err == -1) {
			fprintf(stderr, "%s: login failed\n", __func__);
			return 1;
		} else if (err == 1) {
			fprintf(stderr, "%s: login failed, %s (%i)\n",
					__func__, api_last_errmsg, api_last_code);
			free(token);
			return 1;
		}

		if (save_token(token)) {
			fprintf(stderr, "%s: could not save token\n", __func__);
			free(token);
			return 1;
		}
	} else {
		printf("unable to login: both username and password have to be supplied\n");
		return 1;
	}

	if (api_set_access_token(token)) {
		fprintf(stderr, "%s: could not set access token\n", __func__);
		free(token);
		return 1;
	}
	free(token);
	return 0;
}
static int start_sync(void)
{
	int err = 0;
	pthread_attr_t attr;
	if ((err = pthread_attr_init(&attr))) {
		return err;
	}
	if ((err = pthread_attr_setstacksize(&attr, SYNC_HANDLER_STACKSIZE))) {
		pthread_attr_destroy(&attr);
		return err;
	}
	if ((err = pthread_create(&sync_handler, &attr, sync_main, NULL))) {
		perror(NULL);
	}
	pthread_attr_destroy(&attr);
	return err;
}

int parse_options(int argc, char *const argv[])
{
	options.username = NULL;
	options.pass = NULL;

	int ret = 0;
	const char *optstr = ":u:p:n:";
	for (int c = getopt(argc, argv, optstr); c != -1; c = getopt(argc, argv, optstr)) {
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
		case 'u':
			options.username = s;
			break;
		case 'p':
			options.pass = s;
			break;
		default:
			assert(0);
		}
	}
	return 0;

err_cleanup_options:
	free(options.username);
	free(options.pass);
	return ret;
}

static void cleanup(void);

static int handle_sync(uimode_t mode)
{
	int err;
	switch (mode) {
	case MODE_ROOM_MENU:
		if ((err = room_menu_handle_sync()))
			return err;
		break;
	case MODE_ROOM:
		if ((err = room_handle_sync()))
			return err;
		break;
	default:
		assert(0);
	}
	return 0;
}
static int handle_event(uimode_t *mode, int ch)
{
	int err;
	switch (*mode) {
	case MODE_ROOM_MENU:
		if ((err = room_menu_handle_event(ch, mode)))
			return err;
		break;
	case MODE_ROOM:
		if ((err = room_handle_event(ch, mode)))
			return err;
		break;
	default:
		assert(0);
	}

	return 0;
}

static void setup(void)
{
	if ((flog = open("smclog.txt", O_CREAT | O_WRONLY | O_TRUNC)) < 0)
		exit(1);

	int err;
	if (api_init()) {
		fprintf(stderr, "%s: Could not initialize api\n", __func__);
		close(flog);
		exit(1);
	}

	if (ensure_login()) {
		fprintf(stderr, "%s: Failed to login\n", __func__);
		goto err_api_free;
	}

	list_init(&smc_rooms[ROOMTYPE_JOINED]);
	list_init(&smc_rooms[ROOMTYPE_INVITED]);
	list_init(&smc_rooms[ROOMTYPE_LEFT]);
	if (update()) {
		fprintf(stderr, "%s: Failed to perform initial sync\n", __func__);
		goto err_rooms_free;
	}

	if (start_sync()) {
		fprintf(stderr, "%s: Failed to start sync thread\n", __func__);
		goto err_rooms_free;
	}

	if (!initscr()) {
		fprintf(stderr, "%s: failed to init ncurses\n", __func__);
		goto err_sync_stop;
	}

	if (cbreak() || noecho() || nodelay(stdscr, TRUE) || curs_set(0) == ERR) {
		fprintf(stderr, "%s: failed to init ncurses\n", __func__);
		goto err_curses_free;
	}

	if (room_menu_init()) {
		fprintf(stderr, "%s: failed to initialize room menu\n", __func__);
		goto err_curses_free;
	}
	return;

err_interfaces_free:
	room_cleanup();
	room_menu_cleanup();
err_curses_free:
	endwin();
err_sync_stop:
	smc_terminate = 1;
	pthread_join(sync_handler, NULL);
err_rooms_free:
	list_free(&smc_rooms[ROOMTYPE_LEFT], room_t, entry, free_room);
	list_free(&smc_rooms[ROOMTYPE_INVITED], room_t, entry, free_room);
	list_free(&smc_rooms[ROOMTYPE_JOINED], room_t, entry, free_room);
err_api_free:
	api_cleanup();
	close(flog);
	exit(1);
}
static void run(void)
{
	clear();
	refresh();

	room_menu_draw();
	uimode_t mode = MODE_ROOM_MENU;

	int c;
	int ch;
	const struct timespec tsleep = {.tv_sec = 0, .tv_nsec = 10 * 1000 * 1000 };
	struct timespec ts;
	while (1) {
		ch = getch();

		pthread_mutex_lock(&smc_synclock);
		int sync = smc_sync_avail;
		pthread_mutex_unlock(&smc_synclock);

		if (ch != ERR) {
			if (handle_event(&mode, ch))
				goto err_cleanup;
		} else if (sync) {
			if (handle_sync(mode))
				goto err_cleanup;

			pthread_mutex_lock(&smc_synclock);
			smc_sync_avail = 0;
			pthread_mutex_unlock(&smc_synclock);
		}

		pthread_mutex_lock(&smc_synclock);
		int terminate = smc_terminate;
		pthread_mutex_unlock(&smc_synclock);
		if (terminate)
			break;

		ts = tsleep;
		while (nanosleep(&ts, &ts) == -1) {
			assert(errno == EINTR);
		}
	}
	return;

err_cleanup:
	cleanup();
	exit(1);
}
static void cleanup(void)
{
	room_menu_cleanup();
	endwin();

	smc_terminate = 1;
	pthread_join(sync_handler, NULL);

	list_free(&smc_rooms[ROOMTYPE_LEFT], room_t, entry, free_room);
	list_free(&smc_rooms[ROOMTYPE_INVITED], room_t, entry, free_room);
	list_free(&smc_rooms[ROOMTYPE_JOINED], room_t, entry, free_room);

	api_cleanup();

	close(flog);
}
int main(int argc, char *argv[])
{
	parse_options(argc, argv);

	setup();
	run();
	cleanup();
	return 0;
}
