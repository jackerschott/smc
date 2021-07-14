#include <assert.h>
#include <string.h>

#include "lib/htermbox.h"
#include "msg/smc.h"

#define PRINT_BUFSIZE 64

#define X_TO_WIN(w, _x) ((_x) - (w)->x)
#define Y_TO_WIN(w, _y) ((_y) - (w)->y)
#define X_FROM_WIN(w, _x) ((_x) + (w)->x)
#define Y_FROM_WIN(w, _y) ((_y) + (w)->y)

int xcursor = 0;
int ycursor = 0;
int resize_request = 0;

tb_win_t tb_screen;

int is_valid_window(tb_win_t *win)
{
	return win->width >= 0 && win->height >= 0
		&& win->x >= 0 && win->x + win->width <= tb_width()
		&& win->y >= 0 && win->y + win->height <= tb_height();
}

int tbh_init(void)
{
	int r = tb_init();

	tb_screen.x = 0;
	tb_screen.y = 0;
	tb_screen.width = tb_width();
	tb_screen.height = tb_height();
	tb_screen.wrap = 0;

	return r;
}
void tbh_shutdown(void)
{
	tb_shutdown();
}
int tbh_poll_event(struct tb_event *ev)
{
	int r = tb_poll_event(ev);
	resize_request = ev->type == TB_EVENT_RESIZE;
	return r;
}
int tbh_peek_event(struct tb_event *ev, int timeout)
{
	int r = tb_peek_event(ev, timeout);
	resize_request = ev->type == TB_EVENT_RESIZE;
	return r;
}
void tbh_clear(void)
{
	tb_clear();
	if (resize_request) {
		tb_screen.width = tb_width();
		tb_screen.height = tb_height();

		xcursor = CLAMP(xcursor, 0, tb_screen.width);
		ycursor = CLAMP(ycursor, 0, tb_screen.height);

		resize_request = 0;
	}
}

void tb_getxy(int *x, int *y)
{
	*x = xcursor;
	*y = ycursor;
}
void tb_move(int x, int y)
{
	tb_wmove(&tb_screen, x, y);
}
void tb_wmove(tb_win_t *win, int x, int y)
{
	assert(x >= 0 && x < win->width && y >= 0 && y < win->height);

	xcursor = win->x + x;
	ycursor = win->y + y;
}

void tb_chattr(tb_win_t *win, int width, int height, uint32_t fg, uint32_t bg)
{
	int x = X_TO_WIN(win, xcursor);
	int y = Y_TO_WIN(win, ycursor);
	return tb_mvchattr(win, x, y, width, height, fg, bg);
}
void tb_mvchattr(tb_win_t *win, int x, int y, int width, int height, uint32_t fg, uint32_t bg)
{
	assert(width >= 0 && height >= 0);
	assert(x >= 0 && x + width <= tb_width() && y >= 0 && y + height <= tb_height());

	struct tb_cell *cells = tb_cell_buffer();
	for (int i = x; i < x + width; ++i) {
		for (int j = y; j < y + height; ++j) {
			cells[j * width + i].fg = fg;
			cells[j * width + i].bg = bg;
		}
	}
}
void tb_hline(tb_win_t *win, int width, uint32_t ch)
{
	assert(tb_height() > 0);
	assert(xcursor + width <= tb_width());

	for (int i = 0; i < width; ++i)
	{
		tb_change_cell(xcursor + i, ycursor, ch, TB_DEFAULT, TB_DEFAULT);
	}
}
void tb_vline(tb_win_t *win, int height, uint32_t ch)
{
	assert(tb_width() > 0);
	assert(ycursor + height <= tb_height());

	for (int i = 0; i < height; ++i)
	{
		tb_change_cell(xcursor, ycursor + i, ch, TB_DEFAULT, TB_DEFAULT);
	}
}

int tb_printf(tb_win_t *win, uint32_t fg, uint32_t bg, const char *fmt, ...)
{
	va_list args;

	int x = X_TO_WIN(win, xcursor);
	int y = Y_TO_WIN(win, ycursor);
	va_start(args, fmt);
	return tb_v_mvprintf(win, x, y, fg, bg, fmt, args);
	va_end(args);
}
int tb_mvprintf(tb_win_t *win, int x, int y, uint32_t fg, uint32_t bg, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	return tb_v_mvprintf(win, x, y, fg, bg, fmt, args);
	va_end(args);
}
int tb_v_printf(tb_win_t *win, uint32_t fg, uint32_t bg, const char *fmt, va_list args)
{
	int x = X_TO_WIN(win, xcursor);
	int y = Y_TO_WIN(win, ycursor);
	return tb_v_mvprintf(win, x, y, fg, bg, fmt, args);
}
int tb_v_mvprintf(tb_win_t *win, int x, int y, uint32_t fg,
		uint32_t bg, const char *fmt, va_list args)
{
	int w = win->width;
	int h = win->height;
	assert(is_valid_window(win));
	if (x < 0 && x >= w && y < 0 && y >= h)
		return 1;

	size_t sz = PRINT_BUFSIZE;
	char *s = malloc(sz);
	if (!s)
		return -1;

	while (1) {
		size_t n = vsnprintf(s, sz, fmt, args);
		if (n < sz)
			break;

		sz += PRINT_BUFSIZE;
		char *snew = realloc(s, sz);
		if (!snew) {
			free(s);
			return -1;
		}
		s = snew;
	}

	size_t l = strlen(s);
	for (int i = 0; i < l; ++i) {
		if (s[i] == '\n' || x > w && win->wrap) {
			x = 0;
			if (++y > h) {
				free(s);
				return 1;
			}
			continue;
		} else if (x > w && !win->wrap) {
			continue;
		}
		assert(s[i] >= ' ' && s[i] <= '~');

		tb_change_cell(win->x + x, win->y + y, s[i], fg, bg);
		++x;
	}
	free(s);

	xcursor = win->x + x;
	ycursor = win->y + y;
	return 0;
}

void tb_werase(tb_win_t *win)
{
	assert(is_valid_window(win));

	for (int i = win->x; i < win->x + win->width; ++i) {
		for (int j = win->y; j < win->y + win->height; ++j) {
			tb_change_cell(i, j, ' ', TB_DEFAULT, TB_DEFAULT);
		}
	}
}

void tb_present_cursor(void)
{
	int x, y;
	tb_getxy(&x, &y);
	tb_set_cursor(x, y);
	tb_present();
}

char tb_char(uint16_t key) {
	switch (key) {
	case TB_KEY_BACKSPACE:
	case TB_KEY_BACKSPACE2:
		return '\b';
	case TB_KEY_ENTER:
		return '\n';
	case TB_KEY_SPACE:
		return ' ';
	case TB_KEY_ESC:
		return '\33';
	default:
		assert(0);
	}
}
