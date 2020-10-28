#include <ctype.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>

#include "http.h"

static size_t strip(const char *s, size_t n, char **o)
{
	if (n == 0) {
		*o = (char *)s;
		return 0;
	}

	char *c = (char *)(s + n - 1);
	while (c - s > 0 && (!isprint(*c) || *c == ' '))
		--c;
	int ns = (c - s) + 1;

	c = (char *)s;
	while (c - s < ns && (!isprint(*c) || *c == ' '))
		++c;
	if (o)
		*o = c;
	return ns - (c - s);
}

static int parse_field(const char *const s, size_t l, struct field *f)
{
	const char *d = strchr(s, ':');
	size_t namelen = d - s;
	if (namelen > HTTP_FIELD_SIZE_MAX - 1 || namelen > l - 1) {
		return 1;
	}

	size_t valuelen = strip(d + 1, l - namelen - 1, (char **)&d);
	if (valuelen > HTTP_FIELD_SIZE_MAX - 1) {
		return 1;
	}

	strncpy(f->name, s, namelen);
	f->name[namelen] = '\0';
	strncpy(f->value, d, valuelen);
	f->value[valuelen] = '\0';
	return 0;
}

static int parse_chunk(const char **s, size_t l, char *buf, size_t bufsize)
{
	const char *e;
	unsigned long n = strtoul(*s, (char **)&e, 16);
	size_t ndig = (e - *s);
	if (ndig == 0 || n == LONG_MIN || n == LONG_MAX) {
		return -1;
	} else if (n == 0) {
		return 1;
	}

	if (*(e++) != '\r' || *(e++) != '\n'
			|| ndig + n + 2 > l
			|| n >= bufsize - strlen(buf)) {
		return -1;
	}
	strncat(buf, e, n);

	e += n;
	if (e[0] != '\r' || e[1] != '\n') {
		return -1;
	}
	*s = e + 2;
	return 0;
}

int http_recv_response(int fd, char **buf, size_t *size)
{
	size_t len = 0;
	size_t remsize = *size;
	struct pollfd pfd = { .fd = fd, .events = POLLIN };
	while (1) {
		int res = poll(&pfd, 1, 3000);
		if (res == -1) {
			return 1;
		} else if (!res) {
			return 1;
		}

		size_t readlen = recv(fd, *buf + len, remsize, 0);
		if (readlen == -1) {
			return 1;
		}
		len += readlen;
		remsize -= readlen;

		if (len >= 4 && strncmp(*buf + len - 4, "\r\n\r\n", 4) == 0) {
			break;
		}

		if (remsize == 0) {
			char *bufnew = realloc(*buf, len + HTTP_RESP_READSIZE);
			if (!bufnew)
				return 1;
			*buf = bufnew;
			remsize = HTTP_RESP_READSIZE;
		}
	}
	(*buf)[len] = '\0';
	*size = len + remsize;
	return 0;
}

int http_parse_response(const char *const resp, struct header *head, char *body, size_t bodysize)
{
	const char *c = resp;

	if (strncmp(c, "HTTP/", sizeof("HTTP/") - 1) != 0) {
		return 1;
	}
	c += sizeof("HTTP/") - 1;

	if (strncmp(c, "1.1", sizeof("1.1") - 1) != 0) {
		return 1;
	}
	strncpy(head->version, c, 3);
	head->version[3] = '\0';
	c += 3;

	if (*(c++) != ' ') {
		return 1;
	}

	if (!isdigit(c[0]) || !isdigit(c[1]) || !isdigit(c[2])) {
		return 1;
	}
	strncpy(head->status, c, 3);
	head->status[3] = '\0';
	c += 3;

	if (*(c++) != ' ') {
		return 1;
	}

	c = strchr(c, '\r') + 1;
	if (!c || *(c++) != '\n') {
		return 1;
	}

	int i;
	for (i = 0; c[0] != '\r'; ++i) {
		const char *e = strchr(c, '\r');
		if (!e && e[1] != '\n') {
			return 1;
		}

		if (i >= HTTP_FIELD_NUM_MAX || parse_field(c, e - c, &head->fields[i])) {
			return 1;
		}
		c = e + 2;
	}
	if (c[1] != '\n') {
		return 1;
	}
	c += 2;
	head->nfields = i;

	const char *b = c;
	c = strstr(c, "\r\n\r\n");
	if (!c) {
		return 1;
	}

	const size_t l = c - b;
	if (l >= bodysize) {
		return 1;
	}
	strncpy(body, b, l);
	body[l] = '\0';
	return 0;
}

int http_parse_chunked(const char *s, size_t l, char* buf, size_t bufsize)
{
	const char *e = s;
	int r;
	while ((r = parse_chunk(&e, l, buf, bufsize)) != 1) {
		if (r == -1)
			return -1;

		size_t off = e - s;
		s += off;
		l -= off;
		if (l == 0)
			return 0;
	}
	return 0;
}
