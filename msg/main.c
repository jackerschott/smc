#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ncurses.h>

#include "api.h"
#include "list.h"
#include "menu.h"
#include "smc.h"
#include "room.h"
#include "state.h"
#include "ui.h"

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

static void setup(void)
{
	int err;
	if (api_init()) {
		fprintf(stderr, "%s: Could not initialize api\n", __func__);
		exit(1);
	}
	if (ensure_login()) {
		fprintf(stderr, "%s: Failed to login\n", __func__);
		goto err_api_free;
	}

	if (!initscr()) {
		fprintf(stderr, "%s: failed to init ncurses\n", __func__);
		goto err_api_free;
	}
	if (cbreak() == ERR) {
		fprintf(stderr, "%s: could not set cbreak\n", __func__);
		goto err_ncurses_free;
	}
	if (noecho() == ERR) {
		fprintf(stderr, "%s: could not set noecho\n", __func__);
		goto err_ncurses_free;
	}
	if (curs_set(0) == ERR) {
		fprintf(stderr, "%s: could not hide cursor\n", __func__);
		goto err_ncurses_free;
	}

	if (room_menu_init()) {
		fprintf(stderr, "%s: failed to initialize room menu\n", __func__);
		goto err_ncurses_free;
	}

	if (room_init()) {
		fprintf(stderr, "%s: failed to initialize room interface", __func__);
		room_menu_cleanup();
		goto err_ncurses_free;
	}
	return;

err_interfaces_free:
	room_cleanup();
	room_menu_cleanup();
err_ncurses_free:
	endwin();
err_api_free:
	api_cleanup();
	exit(1);
}
static void run(void)
{
	refresh();

	room_menu_draw();
	uimode_t mode = MODE_ROOM_MENU;

	int c;
	int err;
	while (1) {
		c = getch();
		switch (mode) {
		case MODE_ROOM_MENU:
			if ((err = room_menu_handle_key(c, &mode)))
				goto err_cleanup;
			break;
		case MODE_ROOM:
			if ((err = room_handle_key(c, &mode)))
				goto err_cleanup;
			break;
		default:
			assert(0);
		}
	}

err_cleanup:
	cleanup();
	return;
}
static void cleanup(void)
{
	room_cleanup();
	room_menu_cleanup();
	endwin();
	api_cleanup();
}
int main(int argc, char *argv[])
{
	parse_options(argc, argv);

	setup();
	run();
	cleanup();
	return 0;
}
