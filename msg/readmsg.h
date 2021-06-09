#ifndef READMSG_H
#define READMSG_H

#include <stddef.h>

void readline_init(void);

void readline_forward(char c);
void readline_start(void (*procline)(char *), void (*procredisplay)(char *, char *, size_t));
void readline_stop(void);

#endif /* READMSG_H */
