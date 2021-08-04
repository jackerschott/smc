#ifndef CURS_H
#define CURS_H

#include <ncurses.h>

int nprintw(int n, const char *fmt, ...);
int wnprintw(WINDOW *win, int n, const char *fmt, ...);
int vwnprintw(WINDOW *win, int n, const char *fmt, va_list args);

#endif /* CURS_H */
