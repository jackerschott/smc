#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <openssl/ossl_typ.h>

#define HTTP_REASON_SIZE_MAX 16
#define HTTP_FIELD_NUM_MAX 128
#define HTTP_FIELD_SIZE_MAX 1024
#define HTTP_TARGET_SIZE_MAX 128

#define HTTP_RESP_READSIZE 4096UL

struct field {
	char name[HTTP_FIELD_SIZE_MAX];
	char value[HTTP_FIELD_SIZE_MAX];
};

struct respheader {
	char version[4];
	char status[4];
	struct field fields[HTTP_FIELD_NUM_MAX];
	int nfields;
};

struct reqheader {
	char version[4];
	char type[8];
	char target[HTTP_TARGET_SIZE_MAX];
	struct field fields[HTTP_FIELD_NUM_MAX];
	int nfields;
};

int http_recv_response(int fd, char **buf, size_t *size);
int http_recv_response_ssl(SSL *ssl, char **buf, size_t *size);
int http_parse_response(const char *const resp, struct respheader *head, char *body, size_t bodysize);
int http_parse_chunked(const char *s, size_t l, char* buf, size_t bufsize);
int http_create_request(struct reqheader* head, const char *body, char *req, size_t reqsize);

#endif /* HTTP_H */
