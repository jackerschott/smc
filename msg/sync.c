#include "mtx/mtx.h"
#include "msg/smc.h"
#include "msg/sync.h"

#define SYNC_TIMEOUT 1000

int smc_sync_avail = 0;
pthread_mutex_t smc_synclock;

int update(void)
{
	if (mtx_sync(smc_session, SYNC_TIMEOUT))
		return 1;

	return 0;
}

static void setup(void)
{
	// nothing to do (yet)
}
static void cleanup(void)
{
	// nothing to do (yet)
}
void *sync_main(void *args)
{
	setup();

	struct timespec ts = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
	while (1) {
		pthread_mutex_lock(&smc_synclock);
		int err = update();
		pthread_mutex_unlock(&smc_synclock);
		if (err)
			goto err_cleanup;

		pthread_mutex_lock(&smc_synclock);
		smc_sync_avail = 1;
		pthread_mutex_unlock(&smc_synclock);

		pthread_mutex_lock(&smc_synclock);
		int terminate = smc_terminate;
		pthread_mutex_unlock(&smc_synclock);
		if (terminate)
			break;
	}

err_cleanup:
	pthread_mutex_lock(&smc_synclock);
	smc_terminate = 1;
	pthread_mutex_unlock(&smc_synclock);
	cleanup();
	return 0;
}
