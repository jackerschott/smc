#ifndef HJSON_H
#define HJSON_H

#include <stdint.h>
#include <json-c/json.h>

#include "lib/list.h"

int json_get_int_(const json_object *obj, const char *key, int32_t *i);
int json_get_int64_(const json_object *obj, const char *key, int64_t *i);
int json_get_uint64_(const json_object *obj, const char *key, uint64_t *i);
int json_get_string_(const json_object *obj, const char *key, const char **str);
int json_dup_string_(const json_object *obj, const char *key, char **str);
int json_get_bool_(const json_object *obj, const char *key, int *b);
int json_get_enum_(const json_object *obj, const char *key,
		int *e, int n, char **strs);
int json_dup_string_array_(const json_object *obj, const char *key, char ***_strs);
int json_dup_object_(const json_object *obj, const char *key, json_object **o);

int json_add_int_(json_object *obj, const char *key, int i);
int json_add_string_(json_object *obj, const char *key, const char *str);
int json_add_bool_(json_object *obj, const char *key, int b);
int json_add_enum_(json_object *obj, const char *key, int e, char **strs);
int json_add_string_array_(json_object *obj, const char *key, char **str);
int json_add_array_(json_object *obj, const char *key, json_object **array);
int json_add_object_(json_object *obj, const char *key, json_object **o);

int json_array_add_string_(json_object *obj, const char *str);

#endif /* HJSON_H */
