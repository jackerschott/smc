#ifndef SYNC_H
#define SYNC_H

#include <pthread.h>

#include "lib/list.h"

typedef enum {
	ROOMTYPE_JOINED,
	ROOMTYPE_INVITED,
	ROOMTYPE_LEFT,
	ROOMTYPE_NUM,
} room_type_t;

extern listentry_t smc_rooms[ROOMTYPE_NUM];
extern int smc_sync_avail;

extern pthread_mutex_t smc_synclock;

int update(void);
void *sync_main(void *args);

#endif /* SYNC_H */
