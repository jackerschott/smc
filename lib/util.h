#ifndef UTIL_H
#define UTIL_H

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define CLAMP(x, a, b) MIN(MAX(x, a), b)

#define ARRNUM(x) (sizeof(x) / sizeof((x)[0]))
#define STRLEN(x) (sizeof(x) - 1)

#define OFFSET(TYPE, MEMBER) \
	((size_t)&((TYPE *)0)->MEMBER)
#define CONTAINER(ptr, type, member) \
	((type *)((void *)(ptr) - OFFSET(type, member)))

#define UNUSED(x) (void)(x)

#endif /* UTIL_H */
