#ifndef MTX_DEVICES_H
#define MTX_DEVICES_H

#include <json-c/json_types.h>
#include <olm/olm.h>

#include "lib/list.h"
#include "mtx/encryption.h"

typedef struct {
	mtx_listentry_t entry;

	char *id;
	char *signkey;
	char *identkey;
	mtx_listentry_t otkeys;
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
device_t *create_own_device(OlmAccount *account, const char *id);
int verify_device_object(json_object *obj, const char *userid,
		const char *devid, device_t *prevdev);
int update_device(device_t *dev, const json_object *obj);
device_t *find_device(device_list_t *devlist, char *devid);

int init_device_lists(mtx_listentry_t *devices, const mtx_listentry_t *devtrackinfos);
int update_device_lists(mtx_listentry_t *devices, const json_object *obj);
device_list_t *find_device_list(mtx_listentry_t *devices, char *owner);

int get_device_otkey_counts(const json_object *obj, mtx_listentry_t *counts);

int get_device_tracking_infos(mtx_listentry_t *devices, mtx_listentry_t *devtrackinfos);

#endif /* MTX_DEVICES_H */
