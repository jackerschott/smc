#ifndef SMC_H
#define SMC_H

#include "errno.h"

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define CLAMP(x, a, b) MIN(MAX(x, a), b)

#define ARRNUM(x) (sizeof(x) / sizeof((x)[0]))
#define STRLEN(x) (sizeof(x) - 1)

#define SYSERR() do { \
	fprintf(stderr, "%s:%d/%s: ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, "%i ", errno); \
	perror(NULL); \
} while (0);

#define OFFSET(TYPE, MEMBER) \
	((size_t)&((TYPE *)0)->MEMBER)
#define CONTAINER(ptr, type, member) \
	((type *)((void *)(ptr) - OFFSET(type, member)))

#define UNUSED(x) (void)(x)

#endif /* SMC_H */
