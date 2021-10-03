#ifndef SMC_H
#define SMC_H

#include <errno.h>
#include <stdio.h>

#include <pthread.h>

#include "mtx/types.h"

extern mtx_session_t *smc_session;
extern mtx_room_t *smc_cur_room;

extern int smc_terminate;

extern int flog;

#endif /* SMC_H */
