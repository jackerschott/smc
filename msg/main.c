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
#include <sqlite3.h>

#include "lib/list.h"
#include "msg/menu.h"
#include "msg/smc.h"
#include "msg/sync.h"
#include "msg/room.h"
#include "msg/ui.h"
#include "mtx/mtx.h"

#define SYNC_HANDLER_STACKSIZE (4096 * 64)

mtx_session_t *smc_session;
mtx_room_t *smc_cur_room;

pthread_t sync_handler;
int smc_terminate = 0;

int flog;

struct {
	char *username;
	char *pass;
	char *name;
} options;

#define ACCESSTOKEN_FILE_PATH (CONFIG_DIR "accesstoken")
#define DEVID_FILE_PATH (CONFIG_DIR "device_id")
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
static int save_config(const char *filepath, const char *val)
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
static int get_config(const char *filepath, char **val)
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

static int start_sync(void)
{
	if (pthread_mutex_init(&smc_synclock, NULL))
		return 1;

	pthread_attr_t attr;
	if (pthread_attr_init(&attr)) {
		pthread_mutex_destroy(&smc_synclock);
		return 1;
	}
	if (pthread_attr_setstacksize(&attr, SYNC_HANDLER_STACKSIZE)) {
		pthread_attr_destroy(&attr);
		pthread_mutex_destroy(&smc_synclock);
		return 1;
	}
	if (pthread_create(&sync_handler, &attr, sync_main, NULL)) {
		pthread_attr_destroy(&attr);
		pthread_mutex_destroy(&smc_synclock);
		return 1;
	}
	pthread_attr_destroy(&attr);
	return 0;
}
static void stop_sync(void)
{
	pthread_mutex_lock(&smc_synclock);
	smc_terminate = 1;
	pthread_mutex_unlock(&smc_synclock);

	pthread_join(sync_handler, NULL);

	pthread_mutex_destroy(&smc_synclock);
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

static int initialize_matrix_account(void)
{
	if (mtx_init())
		return 1;

	char *token = NULL;
	if (get_config(ACCESSTOKEN_FILE_PATH, &token)) {
		mtx_cleanup();
		return 1;
	}

	char *devid = NULL;
	if (get_config(DEVID_FILE_PATH, &devid)) {
		mtx_cleanup();
		return 1;
	}

	smc_session = mtx_new_session();
	if (!smc_session) {
		mtx_cleanup();
		return 1;
	}
	assert(!token && !devid || token && devid);

	if (!token) {
		mtx_id_t *id = mtx_create_id_user(options.username);
		if (!id)
			goto err_matrix_cleanup;

		if (mtx_login_password(smc_session, "localhost:8080",
					id, options.pass, devid, NULL)) {
			mtx_free_id(id);
			goto err_matrix_cleanup;
		}
		mtx_free_id(id);

		if (save_config(ACCESSTOKEN_FILE_PATH, mtx_accesstoken(smc_session)))
			goto err_matrix_cleanup;
		
		if (save_config(DEVID_FILE_PATH, mtx_device_id(smc_session)))
			goto err_matrix_cleanup;
	} else {
		if (mtx_recall_past_session(smc_session, "localhost:8080", token, devid))
			goto err_matrix_cleanup;
	}

	if (update()) {
		fprintf(stderr, "%s: Failed to perform initial sync\n", __func__);
		mtx_free_session(smc_session);
		return 1;
	}

	return 0;

err_matrix_cleanup:
	mtx_free_session(smc_session);
	mtx_cleanup();
	return 1;
}
static void cleanup_matrix_account(void)
{
	mtx_free_session(smc_session);
	mtx_cleanup();
}

static void setup(void)
{
	if ((flog = open("smclog.txt", O_CREAT | O_WRONLY | O_TRUNC)) < 0)
		exit(1);

	if (initialize_matrix_account()) {
		fprintf(stderr, "%s: failed to init matrix account\n", __func__);
		close(flog);
		exit(1);
	}

	if (!initscr()) {
		fprintf(stderr, "%s: failed to init ncurses\n", __func__);
		goto err_matrix_account_free;
	}

	if (cbreak() || noecho() || nodelay(stdscr, TRUE) || curs_set(0) == ERR) {
		fprintf(stderr, "%s: failed to init ncurses\n", __func__);
		goto err_curses_free;
	}

	if (room_menu_init()) {
		fprintf(stderr, "%s: failed to initialize room menu\n", __func__);
		goto err_curses_free;
	}

	if (start_sync()) {
		fprintf(stderr, "%s: Failed to start sync thread\n", __func__);
		goto err_room_menu_free;
	}
	return;

err_room_menu_free:
	room_menu_cleanup();
err_curses_free:
	endwin();
err_matrix_account_free:
	cleanup_matrix_account();
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
	stop_sync();

	room_menu_cleanup();
	endwin();

	cleanup_matrix_account();

	dprintf(flog, "%s\n", "cleanup");
	close(flog);
}
int main(int argc, char *argv[])
{
	parse_options(argc, argv);

	setup();
	run();
	cleanup();

	exit_curses(0);
}
