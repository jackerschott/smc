#ifndef INPUT_LINE_H
#define INPUT_LINE_H

#include "lib/htermbox.h"

void input_line_start(char *prompt, int (*handler)(char *));
void input_line_stop(void);
int input_line_handle_event(struct tb_event *ev);
int input_line_is_active(void);

int input_line_print(const char *fmt, ...);
void input_line_clear(void);
void input_line_draw(void);

void input_line_init(void);
void input_line_cleanup(void);

#endif /* INPUT_LINE_H */
