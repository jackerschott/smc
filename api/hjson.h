#ifndef HJSON_H
#define HJSON_H

#include <stdint.h>
#include <json-c/json.h>

int get_object_as_int(const json_object *obj, const char *key, int32_t *i);
int get_object_as_int64(const json_object *obj, const char *key, int64_t *i);
int get_object_as_uint64(const json_object *obj, const char *key, uint64_t *i);
int get_object_as_string(const json_object *obj, const char *key, char **str);
int get_object_as_bool(const json_object *obj, const char *key, int *b);
int get_object_as_enum(const json_object *obj, const char *key, int *e, int n, const char **strs);

void object_add_enum(json_object *obj, const char *key, int e, const char **strs);

#endif /* HJSON_H */
