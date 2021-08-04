#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "lib/hcurs.h"

#define PRINT_BUFSIZE 64

int nprintw(int n, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int err = vwnprintw(stdscr, n, fmt, args);
	va_end(args);
	return err;
}
int wnprintw(WINDOW *win, int n, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int err = vwnprintw(win, n, fmt, args);
	va_end(args);
	return err;
}

int vwnprintw(WINDOW *win, int n, const char *fmt, va_list args)
{
	size_t sz = PRINT_BUFSIZE;
	char *s = malloc(sz);
	if (!s)
		return ERR;

	while (1) {
		size_t n = vsnprintf(s, sz, fmt, args);
		if (n < sz)
			break;

		sz += PRINT_BUFSIZE;
		char *snew = realloc(s, sz);
		if (!snew) {
			free(s);
			return ERR;
		}
		s = snew;
	}

	return waddnstr(win, s, n);
}

//int addstr_wordwrap(WINDOW *win, const char *s)
//{
//	int y, x;
//	getyx(win, y, x);
//
//	int w, h;
//	getmaxyx(win, h, w);
//
//	const char *c0 = s;
//	const char *c = s;
//	while ((c = strchr(s, ' '))) {
//		if ((c - s) ) {
//
//		}
//	}
//}
