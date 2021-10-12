#include <assert.h>
#include <string.h>
#include <stdio.h> // debug

#include <json-c/json.h>
#include <olm/olm.h>

#include "lib/array.h"
#include "lib/hjson.h"
#include "lib/list.h"
#include "mtx/devices.h"
#include "mtx/encryption.h"

void free_device(device_t *dev)
{
	if (!dev)
		return;

	free(dev->id);
	free(dev->signkey);
	free(dev->identkey);
	mtx_list_free(&dev->otkeys, one_time_key_t, entry, free_one_time_key);
	strarr_free(dev->algorithms);

	free(dev->displayname);

	free(dev);
}
device_t *create_own_device(OlmAccount *account, const char *id)
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

	if (create_device_keys(account, &dev->signkey, &dev->identkey)) {
		free_device(dev);
		return NULL;
	}

	if (create_one_time_keys(account, &dev->otkeys)) {
		free_device(dev);
		return NULL;
	}

	return dev;
}
int verify_device_object(json_object *obj, const char *userid,
		const char *devid, device_t *prevdev)
{
	const char *_userid;
	assert(!json_get_string_(obj, "user_id", &_userid));
	if (strcmp(_userid, userid) != 0)
		return 1;

	const char *_devid;
	assert(!json_get_string_(obj, "device_id", &_devid));
	if (strcmp(_devid, devid) != 0)
		return 1;

	json_object *keys;
	assert(json_object_object_get_ex(obj, "keys", &keys));

	char *signkey;
	if (update_device_keys(keys, &signkey, NULL))
		return 1;
	if (prevdev && strcmp(signkey, prevdev->signkey) != 0)
		return 1;

	const char *_signature = get_signature(obj, userid, devid);
	assert(_signature);

	char signature[strlen(_signature) + 1];
	strcpy(signature, _signature);
	int err = verify_signature(obj, signature, signkey);
	if (err == -1) {
		return -1;
	} else if (err) {
		return 1;
	}

	return 0;
}
int update_device(device_t *dev, const json_object *obj)
{
	if (json_dup_string_(obj, "device_it", &dev->id))
		goto err_free_device;

	if (json_dup_string_array_(obj, "algorithms", &dev->algorithms))
		goto err_free_device;

	json_object *keys;
	if (!json_object_object_get_ex(obj, "keys", &keys))
		goto err_free_device;

	if (update_device_keys(keys, &dev->identkey, &dev->identkey))
		goto err_free_device;

	json_object *usigned;
	if (json_object_object_get_ex(obj, "unsigned", &usigned)) {
		if (json_dup_string_(obj, "device_display_name", &dev->displayname))
			goto err_free_device;
	}

	return 0;

err_free_device:
	free_device(dev);
	return 1;
}
device_t *find_device(device_list_t *devlist, char *devid)
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

void free_device_list(device_list_t *devlist)
{
	free(devlist->owner);
	for (mtx_listentry_t *e = devlist->devices.next; e != &devlist->devices; e = e->next) {
		device_t *dev = mtx_list_entry_content(e, device_t, entry);
		free_device(dev);
	}
	free(devlist);
}
device_list_t *create_device_list(const char *owner)
{
	device_list_t *devlist = malloc(sizeof(*devlist));
	if (!devlist)
		return NULL;
	memset(devlist, 0, sizeof(*devlist));
	mtx_list_init(&devlist->devices);

	if (strrpl(&devlist->owner, owner) == 0) {
		free(devlist);
		return NULL;
	}

	return devlist;
}
int init_device_lists(mtx_listentry_t *devices, const mtx_listentry_t *devtrackinfos)
{
	if (!devtrackinfos)
		return 0;

	for (mtx_listentry_t *e = devtrackinfos->next; e != devtrackinfos; e = e->next) {
		device_tracking_info_t *info = mtx_list_entry_content(e,
				device_tracking_info_t, entry);

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
int update_device_lists(mtx_listentry_t *devices, const json_object *devlists)
{
	char **changed = NULL;
	if (json_dup_string_array_(devlists, "changed", &changed) == -1)
		return 1;

	if (changed) {
		for (size_t i = 0; changed[i]; ++i) {
			device_list_t *devlist = find_device_list(devices, changed[i]);
			if (!devlist) {
				device_list_t *_devlist = create_device_list(changed[i]);
				if (!_devlist)
					return 1;
				devlist = _devlist;

				mtx_list_add(devices, &devlist->entry);
			}

			devlist->dirty = 1;
		}
	}

	char **left = NULL;
	if (json_dup_string_array_(devlists, "left", &left) == -1)
		return 1;

	if (left) {
		for (size_t i = 0; left[i]; ++i) {
			device_list_t *devlist = find_device_list(devices, left[i]);
			assert(devlist);

			mtx_list_del(&devlist->entry);
			free_device_list(devlist);
		}
	}

	return 0;
}
device_list_t *find_device_list(mtx_listentry_t *devices, char *owner)
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

void free_device_tracking_info(device_tracking_info_t *info)
{
	free(info->owner);
	free(info);
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

int get_device_otkey_counts(const json_object *obj, mtx_listentry_t *counts)
{
	return 0;
}
