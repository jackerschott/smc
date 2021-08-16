#include "mtx/mtx.h"
#include "msg/sync.h"

listentry_t smc_rooms[ROOMTYPE_NUM];
room_t *smc_cur_room;
int smc_sync_avail = 0;

pthread_mutex_t smc_synclock;

listentry_t sync_batches;

int update(void)
{
	json_object *resp;
	if (mtx_sync(smc_session, &resp))
		return 1;

	pthread_mutex_lock(&smc_synclock);
	listentry_t *joined = &smc_rooms[ROOMTYPE_JOINED];
	listentry_t *invited = &smc_rooms[ROOMTYPE_INVITED];
	listentry_t *left = &smc_rooms[ROOMTYPE_LEFT];
	if (state_apply_sync_updates(resp, &sync_batches, joined, invited, left)) {
		json_object_put(resp);
		return 1;
	}
	pthread_mutex_unlock(&smc_synclock);

	json_object_put(resp);
	return 0;
}

static void setup(void)
{
	state_init_batch_list();
}
static void cleanup(void)
{
	state_free_batch_list();
}
void *sync_main(void *args)
{
	setup();

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
	pthread_mutex_lock(&smc_synclock);
	smc_terminate = 1;
	pthread_mutex_unlock(&smc_synclock);
	cleanup();
	return 0;
}
