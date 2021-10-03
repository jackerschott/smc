#ifndef SYNC_H
#define SYNC_H

#include <pthread.h>

extern int smc_sync_avail;
extern pthread_mutex_t smc_synclock;

int update(void);
void *sync_main(void *args);

#endif /* SYNC_H */
