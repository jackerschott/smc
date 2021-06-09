#include <assert.h>
#include <string.h>

#include "hjson.h"

int get_object_as_int(const json_object *obj, const char *key, int32_t *i)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;
	*i = json_object_get_int(tmp);
	return 0;
}
int get_object_as_int64(const json_object *obj, const char *key, int64_t *i)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;
	*i = json_object_get_int64(tmp);
	return 0;
}
int get_object_as_uint64(const json_object *obj, const char *key, uint64_t *i)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;
	*i = json_object_get_uint64(tmp);
	return 0;
}
int get_object_as_string(const json_object *obj, const char *key, char **str)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;
	char *s = strdup(json_object_get_string(tmp));
	if (!s)
		return -1;
	*str = s;
	return 0;
}
int get_object_as_bool(const json_object *obj, const char *key, int *b)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;
	*b = (int)json_object_get_boolean(tmp);
	return 0;
}
int get_object_as_enum(const json_object *obj, const char *key, int *e, int n, const char **strs)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;
	const char *s = json_object_get_string(tmp);
	for (int i = 0; i < n; ++i) {
		if (strcmp(strs[i], s) == 0) {
			*e = i;
			return 0;
		}
	}
	assert(0);
}

void object_add_enum(json_object *obj, const char *key, int e, const char **strs)
{
	json_object_object_add(obj, key, json_object_new_string(strs[e]));
}

//int get_object_as_string_array(const json_object *obj, const char *key,
//		size_t *nstrings, char ***strings)
//{
//	int err;
//	json_object *tmp;
//	if (!json_object_object_get_ex(obj, key, &tmp))
//		return 1;
//
//	size_t n = json_object_array_length(tmp);
//	char **strs = malloc(n * sizeof(*strs));
//	memset(strs, 0, n * sizeof(*strs));
//	for (size_t i = 0; i < n; ++i) {
//		json_object *strobj = json_object_array_get_idx(tmp, i);
//
//		char *s = strdup(json_object_get_string(strobj));
//		if (!s) {
//			err = -1;
//			goto err_free;
//		}
//		strs[i] = s;
//	}
//	*nstrings = n;
//	*strings = strs;
//	return 0;
//
//err_free:
//	for (size_t i = 0; i < n; ++i) {
//		free(strs[i]);
//	}
//	return err;
//}
//int get_object_as_int_table(const json_object *obj, const char *key,
//		char ***tablekeys, int **tablevals)
//{
//	json_object *tmp;
//	if (!json_object_object_get_ex(obj, key, &tmp))
//		return 1;
//
//	int n = json_object_object_length(tmp);
//	char **keys = malloc(n * sizeof(*keys));
//	if (!keys)
//		return -1;
//	memset(keys, 0, n * sizeof(*keys));
//
//	int *values = malloc(n * sizeof(*values));
//	if (!values) {
//		free(keys);
//		return -1;
//	}
//
//	int i = 0;
//	json_object_object_foreach(tmp, k, v) {
//		char *key = strdup(k);
//		if (!key)
//			goto err_free;
//		keys[i] = k;
//		values[i] = json_object_get_int(v);
//		++i;
//	}
//	*tablekeys = keys;
//	*tablevals = values;
//	return 0;
//
//err_free:
//	for (int i = 0; i < n; ++i) {
//		free(keys[i]);
//	}
//	free(keys);
//	free(values);
//	return -1;
//}
