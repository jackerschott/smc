#ifndef MTX_UTIL_H
#define MTX_UTIL_H

#include <stddef.h>
#include <stdint.h>

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

int strrpl(char **dest, const char *src);

int str2enum(const char *s, const char **strs, int nstrs);

int getrandom_(void *buf, size_t len);

void base64url(const uint8_t *src, size_t n, uint8_t *dest);

#endif /* MTX_UTIL_H */
