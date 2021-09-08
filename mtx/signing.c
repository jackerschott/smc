#include <assert.h>
#include <string.h>

#include <json-c/json.h>
#include <olm/olm.h>

#include "lib/util.h"
#include "lib/hjson.h"
#include "mtx/signing.h"

const char *algorithm_signing = "ed25519";

static char *get_canonical_json_string(json_object *obj)
{
	return strdup(json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));
}

int sign_json(OlmAccount *account, json_object *obj, const char *userid, const char *keyident)
{
	json_object *usigned = json_object_object_get(obj, "unsigned");
	json_object *signatures = json_object_object_get(obj, "signatures");
	if (!signatures) {
		signatures = json_object_new_object();
		if (!signatures)
			return 1;
	}

	json_object_object_del(obj, "unsigned");
	json_object_object_del(obj, "signatures");

	char *canonical = get_canonical_json_string(obj);
	if (!canonical)
		goto err_free_excluded;

	size_t signlen = olm_account_signature_length(account);
	void *signature = malloc(signlen);
	if (!signature) {
		free(canonical);
		goto err_free_excluded;
	}

	if (olm_account_sign(account, canonical, strlen(canonical),
				signature, signlen) == olm_error()) {
		free(signature);
		free(canonical);
		goto err_free_excluded;
	}
	free(canonical);

	if (usigned && json_object_object_add(obj, "unsigned", usigned)) {
		free(signature);
		goto err_free_excluded;
	}
	if (json_object_object_add(obj, "signatures", signatures)) {
		free(signature);
		json_object_put(signatures);
		return 1;
	}

	json_object *signobj = json_object_new_object();
	if (!signobj) {
		free(signature);
		return 1;
	}
	if (json_object_object_add(obj, userid, signobj)) {
		free(signature);
		json_object_put(signobj);
		return 1;
	}

	char *signkey = malloc(strlen(algorithm_signing) + STRLEN(":") + strlen(keyident));
	if (!signkey) {
		free(signature);
		return 1;
	}
	if (json_add_string_(obj, signkey, signature)) {
		free(signkey);
		free(signature);
		return 1;
	}
	free(signkey);
	free(signature);
	return 0;

err_free_excluded:
	json_object_put(signatures);
	json_object_put(usigned);
	return 1;
}
int check_json_signature(json_object *obj, const char *userid, char **known_algorithms)
{
	json_object *signatures = json_object_object_get(obj, "signatures");
	if (!signatures)
		return 1;

	json_object *signature = json_object_object_get(obj, userid);
	if (!signature)
		return 1;

	json_object_object_foreach(signature, k, v) {
		char *c = strchr(k, ':');
		assert(c);
		size_t n = c - k;

		int found = 0;
		for (size_t i = 0; known_algorithms[i] != NULL; ++i) {
			if (strncmp(k, known_algorithms[i], n)) {
				found = 1;
				break;
			}
		}
	}
	return 0;
}
