#include <assert.h>
#include <stdio.h>

#include <readline/readline.h>
#include <ncurses.h>

#include "smc.h"
#include "readmsg.h"

#define PROMPT "> "

char msg_input;
int msg_input_avail;

void (*process_redisplay)(char *, char *, size_t);

static int readline_getc(FILE *f)
{
	UNUSED(f);

	msg_input_avail = 0;
	return msg_input;
}
static int readline_input_avail(void)
{
	return msg_input_avail;
}
static void readline_redisplay(void)
{
	process_redisplay(rl_display_prompt, rl_line_buffer, STRLEN(PROMPT) + rl_point);
}

void readline_forward(char c)
{
	msg_input = c;
	msg_input_avail = 1;
	rl_callback_read_char();
}

void readline_init(void)
{
	/* Do not interfere with ncurses */
	rl_catch_signals = 0;
	rl_catch_sigwinch = 0;
	rl_deprep_term_function = NULL;
	rl_prep_term_function = NULL;

	rl_change_environment = 0;

	rl_getc_function = readline_getc;
	rl_input_available_hook = readline_input_avail;
	rl_redisplay_function = readline_redisplay;

	/* callback to process input line */
	rl_callback_handler_remove();
}

void readline_start(void (*procline)(char *), void (*procredisplay)(char *, char *, size_t))
{
	process_redisplay = procredisplay;
	rl_callback_handler_install(PROMPT, procline);
}
void readline_stop(void)
{
	rl_callback_handler_remove();
}
