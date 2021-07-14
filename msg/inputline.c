#include <assert.h>
#include <stdarg.h>
#include <stdio.h> /* FILE */

#include <readline/readline.h>

#include "msg/inputline.h"
#include "msg/smc.h"

tb_win_t inputwin;

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

	tb_werase(&inputwin);
	tb_wmove(&inputwin, 0, 0);
	tb_printf(&inputwin, TB_DEFAULT, TB_DEFAULT, "%s%s", rl_display_prompt, rl_line_buffer);
	tb_wmove(&inputwin, xcur, 0);
	tb_present();
	tb_present_cursor();
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
	tb_wmove(&inputwin, 0, 0);
	rl_callback_handler_install(prompt, forward_line);

	handle_line = handler;
	is_active = 1;
}
void input_line_stop(void)
{
	rl_callback_handler_remove();

	tb_werase(&inputwin);
	tb_set_cursor(TB_HIDE_CURSOR, TB_HIDE_CURSOR);
	tb_present();

	is_active = 0;
}
int input_line_handle_event(struct tb_event *ev)
{
	assert(is_active);

	if (ev->type == TB_EVENT_KEY) {
		input = ev->ch ? ev->ch : tb_char(ev->key);
		input_avail = 1;
		rl_callback_read_char();

		return line_handle_err;
	} else if (ev->type == TB_EVENT_RESIZE) {
		input_line_draw();
	}
	return 0;
}
int input_line_is_active(void)
{
	return is_active;
}

int input_line_print(const char *fmt, ...)
{
	va_list args;

	tb_wmove(&inputwin, 0, 0);
	va_start(args, fmt);
	tb_v_printf(&inputwin, TB_DEFAULT, TB_DEFAULT, fmt, args);
	va_end(args);
	tb_present();
	return 0;
}
void input_line_clear(void)
{
	tb_werase(&inputwin);
	tb_present();
}
void input_line_draw(void)
{
	update_line();
}

void input_line_init(void)
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

	inputwin.x = 0;
	inputwin.y = tb_height() - 1;
	inputwin.width = tb_width();
	inputwin.height = 1;
	inputwin.wrap = 0;
}
void input_line_cleanup(void)
{
	/* empty for now */
}
