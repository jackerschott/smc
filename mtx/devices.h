#ifndef MTX_DEVICES_H
#define MTX_DEVICES_H

#include <json-c/json_types.h>
#include <olm/olm.h>

#include "lib/list.h"

extern const char *crypto_algorithms_msg[2];

typedef struct {
	mtx_listentry_t entry;

	char *id;
	json_object *identkeys;
	json_object *otkeys;
	char **algorithms;

	char *displayname;
} device_t;
typedef struct {
	mtx_listentry_t entry;

	char *owner;
	mtx_listentry_t devices;

	int dirty;
} device_list_t;

typedef struct {
	mtx_listentry_t entry;

	char *owner;
	int dirty;
} device_tracking_info_t;

void free_device(device_t *dev);
void free_device_list(device_list_t *devlist);
device_t *create_device(OlmAccount *account, const char *id);

int init_device_lists(mtx_listentry_t *devices, const mtx_listentry_t *devtrackinfos);

int update_device(mtx_listentry_t *devices, char *owner, char *devid, const json_object *devinfo);

int update_device_lists(mtx_listentry_t *devices, const json_object *obj);
int get_device_otkey_counts(const json_object *obj, mtx_listentry_t *counts);

int get_device_tracking_infos(mtx_listentry_t *devices, mtx_listentry_t *devtrackinfos);

json_object *device_keys_to_export_format(const json_object *_keys, const char *devid);
json_object *device_keys_from_export_format(json_object *_keys);

#endif /* MTX_DEVICES_H */
