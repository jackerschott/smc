#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h> // debug

#include "lib/util.h"

#include <sys/random.h>

int strrpl(char **dest, const char *src)
{
	if (!src) {
		*dest = NULL;
		return 0;
	}

	char *s = strdup(src);
	if (!s)
		return 1;

	free(*dest);
	*dest = s;
	return 0;
}

int str2enum(const char *s, const char **strs, int nstrs)
{
	for (int i = 0; i < nstrs; ++i) {
		if (strcmp(strs[i], s) == 0) {
			return i;
		}
	}
	printf("%s\n", s);
	assert(0);
}

int getrandom_(void *buf, size_t len)
{
	void *b = buf;
	size_t remlen = len;
	size_t l;
	while (remlen > 0) {
		if ((l = getrandom(b, remlen, GRND_RANDOM)) == -1)
			return 1;

		b += l;
		remlen -= l;
	}
	assert(remlen == 0);
	return 0;
}

static const char base64url_chars[] = {
'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
'8', '9', '-', '_',
};
void base64url(const uint8_t *src, size_t n, uint8_t *dest)
{
	for (size_t i = 0; i < n / 3; ++i) {
		int v[4];
		v[0] = src[i * 3 + 0] >> 2;
		v[1] = (src[i * 3 + 0] & 0x3) << 4 | src[i * 3 + 1] >> 4;
		v[2] = (src[i * 3 + 1] & 0xf) << 2 | src[i * 3 + 2] >> 6;
		v[3] = src[i * 3 + 2] & 0x3f;
		dest[i * 4 + 0] = base64url_chars[v[0]];
		dest[i * 4 + 1] = base64url_chars[v[1]];
		dest[i * 4 + 2] = base64url_chars[v[2]];
		dest[i * 4 + 3] = base64url_chars[v[3]];
	}

	/*
	size_t i = n / 3;
	size_t k = n % 3;
	if (k == 1) {
		dest[i * 4 + 0] = base64url_chars[src[i * 3 + 0] >> 2];
		dest[i * 4 + 1] = base64url_chars[(src[i * 3 + 0] & 0x3) << 4];
		dest[i * 4 + 2] = '=';
		dest[i * 4 + 3] = '=';
	} else if (k == 2) {
		dest[i * 4 + 0] = base64url_chars[src[i * 3 + 0] >> 2];
		dest[i * 4 + 1] = base64url_chars[(src[i * 3 + 0] & 0x3) << 4 | src[i * 3 + 1] >> 4];
		dest[i * 4 + 2] = base64url_chars[(src[i * 3 + 1] & 0xf) << 2];
		dest[i * 4 + 3] = '=';
	}
	*/
}
