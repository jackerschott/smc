#include <assert.h>
#include <stdarg.h>
#include <stdio.h> /* FILE */
#include <stdlib.h>

#include <readline/readline.h>
#include <ncurses.h>

#include "msg/inputline.h"
#include "msg/smc.h"

WINDOW *inputwin;

int input_avail;
int input;
int is_active;

int line_handle_err;

int line_offset = 0;

int (*handle_line)(char *);
static int readline_getc(FILE *f)
{
	UNUSED(f);

	input_avail = 0;
	return input;
}
static int readline_input_avail(void)
{
	return input_avail;
}

void update_line(void)
{
	int xpoint = rl_point;
	int len = strlen(rl_line_buffer);
	int promptlen = strlen(rl_display_prompt);

	int linelen = COLS - promptlen;
	//line_offset = CLAMP(line_offset, xpoint - linelen + 1, xpoint);
	//if (len > linelen)
	//	line_offset = MIN(line_offset, len - linelen);
	line_offset = MAX(xpoint - linelen + 1, 0);
	assert(line_offset <= len);

	werase(inputwin);
	wmove(inputwin, 0, 0);
	wprintw(inputwin, "%s%s", rl_display_prompt, rl_line_buffer + line_offset);
	wmove(inputwin, 0, promptlen + xpoint - line_offset);
	wrefresh(inputwin);
}
void forward_line(char *line)
{
	rl_callback_handler_remove();

	werase(inputwin);
	curs_set(0);
	wrefresh(inputwin);
	is_active = 0;

	if (!line || input != '\n') {
		line_handle_err = 0;
	} else {
		line_handle_err = handle_line(line);
	}

	delwin(inputwin);
	free(line);
}

int input_line_start(char *prompt, int (*handler)(char *))
{
	WINDOW *win = newwin(1, COLS, LINES - 1, 0);
	if (!win)
		return 1;
	inputwin = win;

	wmove(inputwin, 0, 0);
	curs_set(1);
	rl_callback_handler_install(prompt, forward_line);

	handle_line = handler;
	is_active = 1;
	return 0;
}
int input_line_handle_event(int ch)
{
	assert(is_active);

	if (ch == KEY_RESIZE) {
		input_line_draw();
	} else {
		input = ch;
		input_avail = 1;
		rl_callback_read_char();
	}

	return line_handle_err;
}
int input_line_is_active(void)
{
	return is_active;
}

int input_line_print(const char *fmt, ...)
{
	va_list args;

	wmove(inputwin, 0, 0);
	va_start(args, fmt);
	vw_printw(inputwin, fmt, args);
	va_end(args);
	wrefresh(inputwin);
	return 0;
}
void input_line_draw(void)
{
	mvwin(inputwin, LINES - 1, 0);
	wresize(inputwin, 1, COLS);

	update_line();
}

int input_line_init(void)
{
	/* initialize readline */
	/* do not interfere with ncurses */
	rl_catch_signals = 0;
	rl_catch_sigwinch = 0;
	rl_deprep_term_function = NULL;
	rl_prep_term_function = NULL;

	rl_change_environment = 0;

	rl_getc_function = readline_getc;
	rl_input_available_hook = readline_input_avail;
	rl_redisplay_function = update_line;
	return 0;
}
void input_line_cleanup(void)
{
	/* nothing to do for now */
	rl_clear_history();
}
