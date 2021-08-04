#include <assert.h>
#include <stdarg.h>
#include <stdio.h> /* FILE */

#include <readline/readline.h>
#include <ncurses.h>

#include "msg/inputline.h"
#include "msg/smc.h"

WINDOW *inputwin;

int input_avail;
int input;
int is_active;

int line_handle_err;

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
	size_t xcur = strlen(rl_display_prompt) + rl_point;

	werase(inputwin);
	wmove(inputwin, 0, 0);
	wprintw(inputwin, "%s%s", rl_display_prompt, rl_line_buffer);
	wmove(inputwin, xcur, 0);
	wrefresh(inputwin);
}
void forward_line(char *line)
{
	input_line_stop();

	if (!line) {
		line_handle_err = 0;
		return;
	}

	line_handle_err = handle_line(line);
}

void input_line_start(char *prompt, int (*handler)(char *))
{
	wmove(inputwin, 0, 0);
	curs_set(1);
	rl_callback_handler_install(prompt, forward_line);

	handle_line = handler;
	is_active = 1;
}
void input_line_stop(void)
{
	rl_callback_handler_remove();

	werase(inputwin);
	curs_set(0);
	wrefresh(inputwin);

	is_active = 0;
}
int input_line_handle_event(int ch)
{
	assert(is_active);

	if (ch == TB_EVENT_RESIZE) {
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

	WINDOW *win = newwin(1, COLS, LINES - 1, 0);
	if (!win)
		return 1;
	inputwin = win;

	return 0;
}
void input_line_cleanup(void)
{
	delwin(inputwin);
}
