#include <assert.h>
#include <stdarg.h>
#include <stdio.h> /* FILE */

#include <readline/readline.h>
#include <ncurses.h>

#include "inputline.h"
#include "smc.h"

WINDOW *inputwin;

int input_avail;
char input;
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
	wmove(inputwin, 0, xcur);
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
	curs_set(1);
	refresh();

	wmove(inputwin, 0, 0);
	wrefresh(inputwin);

	rl_callback_handler_install(prompt, forward_line);

	handle_line = handler;
	is_active = 1;
}
void input_line_stop(void)
{
	rl_callback_handler_remove();

	werase(inputwin);
	wrefresh(inputwin);

	curs_set(0);
	refresh();

	is_active = 0;
}
int input_line_handle_key(int c)
{
	assert(is_active);

	input = c;
	input_avail = 1;
	rl_callback_read_char();

	return line_handle_err;
}
int input_line_is_active(void)
{
	return is_active;
}

int input_line_print(const char *fmt, ...)
{
	va_list args;
	
	va_start(args, fmt);
	vw_printw(inputwin, fmt, args);
	va_end(args);

	wrefresh(inputwin);
	return 0;
}
void input_line_clear(void)
{
	werase(inputwin);
	wrefresh(inputwin);
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

	if (!(inputwin = newwin(1, COLS, LINES-1, 0)))
		return -1;

	return 0;
}
void input_line_cleanup(void)
{
	delwin(inputwin);
}
