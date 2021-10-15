#ifndef MTX_TESTS_UTILS_H
#define MTX_TESTS_UTILS_H

#include "mtx/types.h"

extern int testerr;

#define CHECK(x) 							\
	do { 								\
		if (!(x)) { 						\
			fprintf(stderr, "check `%s' failed\n", #x); 	\
			testerr = 1; 					\
		} else { 						\
			testerr = 0; 					\
		} 							\
	} while (0)


int save_config(const char *filepath, const char *val);
int get_config(const char *filepath, char **val);

int change_login(mtx_session_t *session, char *username, int logout);

int run(int argc, char **argv, int (*init_server)(void), int (*test)(void));

#endif /* MTX_TESTS_UTILS_H */
