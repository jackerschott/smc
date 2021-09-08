#include <assert.h>
#include <string.h>

#include "lib/hjson.h"

int json_get_object_as_int_(const json_object *obj, const char *key, int32_t *i)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;
	*i = json_object_get_int(tmp);
	return 0;
}
int json_get_object_as_int64_(const json_object *obj, const char *key, int64_t *i)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;
	*i = json_object_get_int64(tmp);
	return 0;
}
int json_get_object_as_uint64_(const json_object *obj, const char *key, uint64_t *i)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;
	*i = json_object_get_uint64(tmp);
	return 0;
}
int json_get_object_as_string_(const json_object *obj, const char *key, char **str)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;

	const char *_s = json_object_get_string(tmp);
	if (!_s) {
		*str = NULL;
		return 0;
	}

	char *s = realloc(*str, strlen(_s) + 1);
	if (!s)
		return -1;
	strcpy(s, _s);
	*str = s;
	return 0;
}
int json_get_object_as_bool_(const json_object *obj, const char *key, int *b)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;
	*b = (int)json_object_get_boolean(tmp);
	return 0;
}
int json_get_object_as_enum_(const json_object *obj, const char *key,
		int *e, int n, const char **strs)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;

	const char *s = json_object_get_string(tmp);
	assert(s);

	*e = str2enum(s, strs, n);
	return 0;
}
int json_get_object_as_string_array_(const json_object *obj, const char *key, char ***_strs)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;

	if (*_strs) {
		for (size_t i = 0; (*_strs)[i]; ++i) {
			free((*_strs)[i]);
		}
	}

	size_t n = json_object_array_length(obj);
	char **strs = realloc(*_strs, (n + 1) * sizeof(*strs));
	if (!strs)
		return -1;
	memset(strs, 0, (n + 1) * sizeof(*strs));

	for (size_t i = 0; i < n; ++i) {
		json_object *element = json_object_array_get_idx(obj, i);
		char *s = strdup(json_object_get_string(element));
		if (!s)
			goto memerr_free_array;

		strs[i] = s;
	}

	*_strs = strs;
	return 0;

memerr_free_array:
	for (size_t i = 0; strs[i] != NULL; ++i) {
		free(strs[i]);
	}
	return -1;
}
int json_get_object_as_object_(const json_object *obj, const char *key, json_object **o)
{
	json_object *tmp;
	if (!json_object_object_get_ex(obj, key, &tmp))
		return 1;

	json_object_put(*o);

	*o = NULL;
	if (json_object_deep_copy(tmp, o, NULL))
		return -1;

	return 0;
}

int json_add_int_(json_object *obj, const char *key, int i)
{
	json_object *intobj = json_object_new_int(i);
	if (!intobj)
		return 1;

	if (json_object_object_add(obj, key, intobj))
		return 1;

	return 0;
}
int json_add_string_(json_object *obj, const char *key, const char *str)
{
	if (!str)
		return 1;

	json_object *strobj = json_object_new_string(str);
	if (!strobj)
		return 1;

	if (json_object_object_add(obj, key, strobj))
		return 1;

	return 0;
}
int json_add_enum_(json_object *obj, const char *key, int e, const char **strs)
{
	if (!strs)
		return 1;

	json_object *strobj = json_object_new_string(strs[e]);
	if (!strobj)
		return 1;

	if (json_object_object_add(obj, key, strobj))
		return 1;

	return 0;
}
int json_add_string_array_(json_object *obj, const char *key, int n, const char **str)
{
	if (!str)
		return 1;

	json_object *array = json_object_new_array_ext(n);
	if (!array)
		return 1;

	for (int i = 0; i < n; ++i) {
		json_object *strobj = json_object_new_string(str[i]);
		if (!strobj)
			return 1;

		if (json_object_array_add(array, strobj))
			return 1;
	}
	return 0;
}
int json_add_array_(json_object *obj, const char *key, json_object **array)
{
	json_object *_array = json_object_new_object();
	if (!_array)
		return 1;

	if (json_object_object_add(obj, key, _array)) {
		json_object_put(_array);
		return 1;
	}

	*array = _array;
	return 0;
}
int json_add_object_(json_object *obj, const char *key, json_object **o)
{
	json_object *_o = json_object_new_object();
	if (!_o)
		return 1;

	if (json_object_object_add(obj, key, _o)) {
		json_object_put(_o);
		return 1;
	}

	*o = _o;
	return 0;
}

int json_array_add_string_(json_object *obj, const char *str)
{
	if (!str)
		return 1;

	json_object *strobj = json_object_new_string(str);
	if (!strobj)
		return 1;

	if (json_object_array_add(obj, strobj))
		return 1;

	return 0;
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
