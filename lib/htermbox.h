#ifndef HTERMBOX_H
#define HTERMBOX_H

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <termbox.h>

#define TB_TABLE_VLINE L'│'
#define TB_TABLE_HLINE L'─'
#define TB_TABLE_ULCORNER L'┌'
#define TB_TABLE_URCORNER L'┐'
#define TB_TABLE_LLCORNER L'└'
#define TB_TABLE_LRCORNER L'┘'

struct tb_win_t {
	int x;
	int y;
	int width;
	int height;
	int wrap;
};
typedef struct tb_win_t tb_win_t;

extern tb_win_t tb_screen;

int tbh_init(void);
void tbh_shutdown(void);
int tbh_poll_event(struct tb_event *ev);
int tbh_peek_event(struct tb_event *ev, int timeout);
void tbh_clear(void);

void tb_getxy(int *x, int *y);
void tb_move(int x, int y);
void tb_wmove(tb_win_t *win, int x, int y);

void tb_chattr(tb_win_t *win, int width, int height, uint32_t fg, uint32_t bg);
void tb_mvchattr(tb_win_t *win, int x, int y, int width, int height, uint32_t fg, uint32_t bg);
void tb_hline(tb_win_t *win, int width, uint32_t ch);
void tb_vline(tb_win_t *win, int height, uint32_t ch);

int tb_printf(tb_win_t *win, uint32_t fg, uint32_t bg, const char *fmt, ...);
int tb_mvprintf(tb_win_t *win, int x, int y, uint32_t fg, uint32_t bg, const char *fmt, ...);
int tb_v_printf(tb_win_t *win, uint32_t fg, uint32_t bg, const char *fmt, va_list args);
int tb_v_mvprintf(tb_win_t *win, int x, int y,
		uint32_t fg, uint32_t bg, const char *fmt, va_list args);

void tb_werase(tb_win_t *win);

void tb_present_cursor(void);

char tb_char(uint16_t key);

#endif /* HTERMBOX_H */
