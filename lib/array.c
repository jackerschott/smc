#include <string.h>

#include "lib/array.h"
#include "lib/util.h"

char **strarr_new(size_t n)
{
	char **arr = malloc((n + 1) * sizeof(*arr));
	if (!arr)
		return NULL;
	memset(arr, 0, (n + 1) * sizeof(*arr));

	return arr;
}
void strarr_free(char **arr)
{
	if (!arr)
		return;

	for (size_t i = 0; arr[i]; ++i) {
		free(arr[i]);
	}
	free(arr);
}

size_t strarr_num(char **arr)
{
	size_t n = 0;
	for (; arr[n]; ++n);
	return n;
}

char **strarr_dup(char **arr)
{
	size_t n = strarr_num(arr);
	char **newarr = malloc(n * sizeof(*newarr) + 1);
	if (!newarr)
		return NULL;
	memset(newarr, 0, n * sizeof(*newarr) + 1);

	for (size_t i = 0; i < n; ++i) {
		if (strrpl(&newarr[i], arr[i]))
			return NULL;
	}

	return newarr;
}
int strarr_rpl(char ***dest, char **src)
{
	if (!src) {
		*dest = NULL;
		return 0;
	}

	char **arr = strarr_dup(src);
	if (!arr)
		return 1;

	strarr_free(*dest);
	*dest = arr;
	return 0;
}
