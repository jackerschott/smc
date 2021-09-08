#ifndef MTX_SIGNING_H
#define MTX_SIGNING_H

#include <json-c/json_types.h>
#include <olm/olm.h>

int sign_json(OlmAccount *account, json_object *obj, const char *userid, const char *keyident);
int check_json_signature(json_object *obj, const char *userid, char **known_algorithms);

#endif /* MTX_SIGNING_H */
