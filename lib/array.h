#include <stddef.h>
#include <stdlib.h>

char **strarr_new(void);
void strarr_free(char **arr);

size_t strarr_num(char **arr);

char **strarr_dup(char **arr);
int strarr_rpl(char ***dest, char **src);
