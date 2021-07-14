#include "msg/sync.h"
#include "api/api.h"

listentry_t smc_rooms[ROOMTYPE_NUM];
int smc_sync_avail = 0;

pthread_mutex_t smc_synclock;

static int update(void)
{
	listentry_t *joined = &smc_rooms[ROOMTYPE_JOINED];
	listentry_t *invited = &smc_rooms[ROOMTYPE_INVITED];
	listentry_t *left = &smc_rooms[ROOMTYPE_LEFT];
	if (api_sync(joined, invited, left))
		return 1;
	return 0;
}

void *sync_main(void *args)
{
	struct timespec ts = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
	while (1) {
		if (update())
			goto err_cleanup;
		pthread_mutex_lock(&smc_synclock);
		smc_sync_avail = 1;
		pthread_mutex_unlock(&smc_synclock);

		pthread_mutex_lock(&smc_synclock);
		int terminate = smc_terminate;
		pthread_mutex_unlock(&smc_synclock);
		if (terminate)
			break;

		while (1) {
			if (nanosleep(&ts, NULL))
				goto err_cleanup;
			pthread_mutex_lock(&smc_synclock);
			int sync = smc_sync_avail;
			pthread_mutex_unlock(&smc_synclock);
			if (!sync)
				break;
		}
	}

err_cleanup:
	smc_terminate = 1;
	return 0;
}
