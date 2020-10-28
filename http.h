#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define HTTP_REASON_SIZE_MAX 16
#define HTTP_FIELD_NUM_MAX 32
#define HTTP_FIELD_SIZE_MAX 128

#define HTTP_RESP_READSIZE 4096UL

struct field {
	char name[HTTP_FIELD_SIZE_MAX];
	char value[HTTP_FIELD_SIZE_MAX];
};

struct header {
	char version[4];
	char status[4];
	struct field fields[HTTP_FIELD_NUM_MAX];
	int nfields;
};

int http_recv_response(int fd, char **buf, size_t *size);
int http_parse_response(const char *const resp, struct header *head, char *body, size_t bodysize);
int http_parse_chunked(const char *s, size_t l, char* buf, size_t bufsize);

#endif /* HTTP_H */
