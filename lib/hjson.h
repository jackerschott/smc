#ifndef HJSON_H
#define HJSON_H

#include <stdint.h>
#include <json-c/json.h>

#include "lib/list.h"

int json_get_object_as_int_(const json_object *obj, const char *key, int32_t *i);
int json_get_object_as_int64_(const json_object *obj, const char *key, int64_t *i);
int json_get_object_as_uint64_(const json_object *obj, const char *key, uint64_t *i);
int json_get_object_as_string_(const json_object *obj, const char *key, char **str);
int json_get_object_as_bool_(const json_object *obj, const char *key, int *b);
int json_get_object_as_enum_(const json_object *obj, const char *key, int *e, int n, const char **strs);

int json_object_add_int_(json_object *obj, const char *key, int i);
int json_object_add_string_(json_object *obj, const char *key, const char *str);
int json_object_add_enum_(json_object *obj, const char *key, int e, const char **strs);
int json_object_add_string_array_(json_object *obj, const char *key, int n, const char **str);
int json_object_add_array_(json_object *obj, const char *key, json_object **array);
int json_object_add_object_(json_object *obj, const char *key, json_object **o);

int json_array_add_string_(json_object *obj, const char *str);

#endif /* HJSON_H */
