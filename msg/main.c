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
#include "smc.h"
#include "state.h"

listentry_t rooms_joined;
listentry_t rooms_invited;
listentry_t rooms_left;

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
int save_token(char *token)
{
	int ftoken = open(TOKEN_FILE_PATH, O_CREAT | O_WRONLY, 0600);
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

void setup(void)
{
	int err;
	if (api_init()) {
		fprintf(stderr, "%s: Could not initialize api\n", __func__);
		exit(1);
	}
	if (ensure_login()) {
		fprintf(stderr, "%s: Failed to login\n", __func__);
		api_cleanup();
		exit(1);
	}

	list_init(&rooms_joined);
	list_init(&rooms_left);
	list_init(&rooms_invited);
	if (api_sync(&rooms_joined, &rooms_left, &rooms_invited)) {
		fprintf(stderr, "%s: could not synchronize state with server\n", __func__);
		api_cleanup();
		exit(1);
	}

	initscr();
	err = raw();
	if (err == ERR) {
		fprintf(stderr, "%s: could not enter raw mode\n", __func__);
		endwin();
		api_cleanup();
		exit(1);
	}
}
void run(void)
{
	int y, x;
	getmaxyx(stdscr, y, x);

	for (listentry_t *e = rooms_joined.next; e != &rooms_joined; e = e->next) {
		room_t *room = LIST_ENTRY(e, room_t, entry);
		printw("%s   %s   %s\n", room->name, room->topic, room->id);
	}
	refresh();

	getch();
}
void cleanup(void)
{
	endwin();
	api_cleanup();
}
int main(int argc, char *argv[])
{
	setup();
	run();
	cleanup();
	return 0;
}
