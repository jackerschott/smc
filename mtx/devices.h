#ifndef MTX_DEVICES_H
#define MTX_DEVICES_H

#include <json-c/json_types.h>
#include <olm/olm.h>

#include "lib/list.h"

extern const char *crypto_algorithms_msg[2];

typedef struct {
	listentry_t entry;

	char *id;
	json_object *identkeys;
	json_object *otkeys;
	char **algorithms;

	char *displayname;
} device_t;
typedef struct {
	listentry_t entry;

	char *owner;
	listentry_t devices;

	int dirty;
} device_list_t;

typedef struct {
	listentry_t entry;

	char *owner;
	int dirty;
} device_tracking_info_t;

json_object *device_keys_to_export_format(const json_object *_keys, const char *devid);
json_object *device_keys_from_export_format(json_object *_keys);

device_t *create_device(OlmAccount *account, const char *id);

int init_device_lists(listentry_t *devices, const listentry_t *devtrackinfos);

int update_device(listentry_t *devices, char *owner, char *devid, const json_object *devinfo);

int update_device_lists(listentry_t *devices, const json_object *obj);
int get_device_otkey_counts(const json_object *obj, listentry_t *counts);

int get_device_tracking_infos(listentry_t *devices, listentry_t *devtrackinfos);

#endif /* MTX_DEVICES_H */
