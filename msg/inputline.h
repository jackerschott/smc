#ifndef INPUT_LINE_H
#define INPUT_LINE_H

void input_line_start(char *prompt, int (*handler)(char *));
void input_line_stop(void);
int input_line_handle_key(int c);
int input_line_is_active(void);

int input_line_print(const char *fmt, ...);
void input_line_clear(void);

int input_line_init(void);
void input_line_cleanup(void);

#endif /* INPUT_LINE_H */
