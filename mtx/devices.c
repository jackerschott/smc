#include <assert.h>
#include <string.h>
#include <stdio.h> // debug

#include <json-c/json.h>
#include <olm/olm.h>

#include "lib/array.h"
#include "lib/hjson.h"
#include "lib/list.h"
#include "mtx/devices.h"

const char *crypto_algorithms_msg[] = {
	"m.olm.v1.curve25519-aes-sha2",
	"m.megolm.v1.aes-sha2",
};

void free_device(device_t *dev)
{
	free(dev->id);
	json_object_put(dev->identkeys);
	json_object_put(dev->otkeys);
	strarr_free(dev->algorithms);

	free(dev->displayname);

	free(dev);
}
void free_device_list(device_list_t *devlist)
{
	free(devlist->owner);
	for (mtx_listentry_t *e = devlist->devices.next; e != &devlist->devices; e = e->next) {
		device_t *dev = mtx_list_entry_content(e, device_t, entry);
		free_device(dev);
	}
	free(devlist);
}
void free_device_tracking_info(device_tracking_info_t *info)
{
	free(info->owner);
	free(info);
}

static json_object *create_device_keys(OlmAccount *account)
{
	size_t identkeylen = olm_account_identity_keys_length(account);
	char *identkey = malloc(identkeylen + 1);
	if (!identkey)
		return NULL;

	if (olm_account_identity_keys(account, identkey, identkeylen) == olm_error()) {
		free(identkey);
		return NULL;
	}
	identkey[identkeylen] = 0;

	json_object *keys = json_tokener_parse(identkey);
	if (!keys) {
		free(identkey);
		return NULL;
	}
	free(identkey);

	return keys;
}
static json_object *create_one_time_keys(OlmAccount *account)
{
	size_t nkeys = olm_account_max_number_of_one_time_keys(account) / 2;

	size_t rdlen = olm_account_generate_one_time_keys_random_length(account, nkeys);
	void *random = malloc(rdlen);
	if (!random)
		return NULL;

	if (getrandom_(random, rdlen)) {
		free(random);
		return NULL;
	}

	if (olm_account_generate_one_time_keys(account, nkeys, random, rdlen) == olm_error()) {
		free(random);
		return NULL;
	}
	free(random);

	size_t otkeylen = olm_account_one_time_keys_length(account);
	char *otkeys = malloc(otkeylen + 1);
	if (!otkeys)
		return NULL;
	if (olm_account_one_time_keys(account, otkeys, otkeylen) == olm_error()) {
		free(otkeys);
		return NULL;
	}
	otkeys[otkeylen] = 0;

	json_object *keys = json_tokener_parse(otkeys);
	if (!keys) {
		free(otkeys);
		return NULL;
	}

	free(otkeys);
	return keys;
}

device_t *create_device(OlmAccount *account, const char *id)
{
	device_t *dev = malloc(sizeof(*dev));
	if (!dev)
		return NULL;
	memset(dev, 0, sizeof(*dev));
	dev->id = NULL;

	if (strrpl(&dev->id, id)) {
		free_device(dev);
		return NULL;
	}

	json_object *identkeys = create_device_keys(account);
	if (!identkeys) {
		free_device(dev);
		return NULL;
	}
	dev->identkeys = identkeys;

	json_object *otkeys = create_one_time_keys(account);
	if (!otkeys) {
		free_device(dev);
		return NULL;
	}
	dev->otkeys = otkeys;

	return dev;
}

int init_device_lists(mtx_listentry_t *devices, const mtx_listentry_t *devtrackinfos)
{
	if (!devtrackinfos)
		return 0;

	for (mtx_listentry_t *e = devtrackinfos->next; e != devtrackinfos; e = e->next) {
		device_tracking_info_t *info = mtx_list_entry_content(e, device_tracking_info_t, entry);

		device_list_t *devlist = malloc(sizeof(*devlist));
		if (!devlist)
			return 1;

		mtx_list_init(&devlist->devices);
		devlist->dirty = info->dirty;
		devlist->owner = NULL;

		if (strrpl(&devlist->owner, info->owner)) {
			free(devlist);
			return 1;
		}

		mtx_list_add(devices, &devlist->entry);
	}

	return 0;
}

static device_list_t *find_device_list(mtx_listentry_t *devices, char *owner)
{
	device_list_t *devlist = NULL;
	for (mtx_listentry_t *e = devices->next; e != devices; e = e->next) {
		device_list_t *dl = mtx_list_entry_content(e, device_list_t, entry);
		if (strcmp(dl->owner, owner) == 0) {
			devlist = dl;
			break;
		}
	}
	return devlist;
}
static device_t *find_device(device_list_t *devlist, char *devid)
{
	device_t *dev = NULL;
	for (mtx_listentry_t *e = devlist->devices.next; e != &devlist->devices; e = e->next) {
		device_t *d = mtx_list_entry_content(e, device_t, entry);
		if (strcmp(d->id, devid) == 0) {
			dev = d;
			break;
		}
	}
	return dev;
}
int update_device(mtx_listentry_t *devices, char *owner, char *devid, const json_object *devinfo)
{
	device_list_t *devlist = find_device_list(devices, owner);
	if (!devlist) {
		devlist = malloc(sizeof(*devlist));
		if (!devlist)
			return -1;
		devlist->owner = NULL;
		mtx_list_init(&devlist->devices);
		devlist->dirty = 0;

		if (strrpl(&devlist->owner, owner)) {
			free(devlist);
			return -1;
		}

		mtx_list_add(devices, &devlist->entry);
	}

	device_t *dev = find_device(devlist, devid);
	if (!dev) {
		dev = malloc(sizeof(*dev));
		if (!dev)
			return -1;
		memset(dev, 0, sizeof(*dev));
		dev->id = NULL;

		if (strrpl(&dev->id, devid)) {
			free(dev);
			return -1;
		}

		mtx_list_add(&devlist->devices, &dev->entry);
	}

	int err;
	json_object *_keys;
	json_object_object_get_ex(devinfo, "keys", &_keys);

	json_object *keys = device_keys_from_export_format(_keys);
	if (!keys)
		return 1;
	dev->identkeys = keys;

	if (json_get_object_as_string_array_(devinfo, "algorithms", &dev->algorithms) == -1)
		return 1;

	json_object *usigned;
	json_object_object_get_ex(devinfo, "unsigned", &usigned);
	if (usigned && json_get_object_as_string_(devinfo,
				"device_display_name", &dev->displayname) == -1)
		return 1;

	// TODO: check signature

	return 0;
}

int update_device_lists(mtx_listentry_t *devices, const json_object *devlists)
{
	char **changed = NULL;
	if (json_get_object_as_string_array_(devlists, "changed", &changed) == -1)
		return 1;

	char **left = NULL;
	if (json_get_object_as_string_array_(devlists, "left", &left) == -1)
		return 1;

	if (changed) {
		for (size_t i = 0; changed[i]; ++i) {
			device_list_t *devlist = find_device_list(devices, changed[i]);
			assert(devlist);

			devlist->dirty = 1;
		}
	}

	if (left) {
		for (size_t i = 0; left[i]; ++i) {
			device_list_t *devlist = find_device_list(devices, left[i]);
			mtx_list_del(&devlist->entry);
			free_device_list(devlist);
		}
	}

	return 0;
}

int get_device_otkey_counts(const json_object *obj, mtx_listentry_t *counts)
{
	return 0;
}

int get_device_tracking_infos(mtx_listentry_t *devices, mtx_listentry_t *devtrackinfos)
{
	mtx_list_init(devtrackinfos);

	for (mtx_listentry_t *e = devices->next; e != devices; e = e->next) {
		device_list_t *devlist = mtx_list_entry_content(e, device_list_t, entry);

		device_tracking_info_t *info = malloc(sizeof(*info));
		if (!info)
			goto err_free_infos;
		memset(info, 0, sizeof(*info));

		if (strrpl(&info->owner, devlist->owner)) {
			free_device_tracking_info(info);
			goto err_free_infos;
		}

		mtx_list_add(devtrackinfos, &info->entry);
	}

	return 0;

err_free_infos:
	mtx_list_free(devtrackinfos, device_tracking_info_t, entry, free_device_tracking_info);
	return 1;
}

json_object *device_keys_to_export_format(const json_object *_keys, const char *devid)
{
	json_object *keys = json_object_new_object();
	if (!keys)
		return NULL;

	json_object_object_foreach(_keys, k, v) {
		char *key = malloc(strlen(k) + STRLEN(":") + strlen(devid) + 1);
		if (!key) {
			json_object_put(keys);
			return NULL;
		}
		strcpy(key, k);
		strcat(key, ":");
		strcat(key, devid);

		json_object *identkey = NULL;
		if (json_object_deep_copy(v, &identkey, NULL)) {
			free(key);
			json_object_put(keys);
			return NULL;
		}

		if (json_object_object_add(keys, key, identkey)) {
			json_object_put(identkey);
			free(key);
			json_object_put(keys);
			return NULL;
		}
		free(key);
	}

	return keys;
}
json_object *device_keys_from_export_format(json_object *_keys)
{
	json_object *keys = json_object_new_object();
	if (!keys)
		return NULL;

	json_object_object_foreach(_keys, k, v) {
		char *algorithm = strdup(k);
		if (!algorithm) {
			json_object_put(keys);
			return NULL;
		}

		char *c = strchr(algorithm, ':');
		assert(c);
		*c = 0;

		if (json_object_object_add(keys, algorithm, v)) {
			free(algorithm);
			json_object_put(keys);
			return NULL;
		}
		free(algorithm);
	}

	return keys;
}
