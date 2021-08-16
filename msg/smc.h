#ifndef SMC_H
#define SMC_H

#include <errno.h>
#include <stdio.h>

#include <pthread.h>

#include "mtx/mtx.h"

//#define SYSERR() do { \
//	fprintf(stderr, "%s:%d/%s: ", __FILE__, __LINE__, __func__); \
//	fprintf(stderr, "%i ", errno); \
//	perror(NULL); \
//} while (0);

extern mtx_session_t *smc_session;
extern int smc_terminate;

extern int flog;

#endif /* SMC_H */
