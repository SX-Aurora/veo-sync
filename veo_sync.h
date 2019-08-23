#ifndef _VEO_SYNC_H_
#define _VEO_SYNC_H_

#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _veo_barrier_args {
	pthread_barrier_t *barrier;
	int *num_blocked;
};

struct _veo_req_res {
	struct veo_thr_ctxt *ctx;	/* context of current request */
	uint64_t req;			/* request ID */
	uint64_t result;		/* result of VEO async call, might need cast */
	int rc;				/* return code, as returned by veo_call_wait_result() */
};

struct veo_barrier {
	pthread_barrier_t pb;
	struct _veo_barrier_args ba;
	int num_ctxts;
	int is_block;
	int num_blocked;		/* need this for being able to wait for blocking */
	struct _veo_req_res *rr;
};

struct veo_barrier *veo_barrier_create(struct veo_thr_ctxt **ctxts,
				       int num_ctxts, int blocking);
int veo_barrier_set(struct veo_barrier *);
int veo_barrier_wait(struct veo_barrier *);
int veo_barrier_unblock(struct veo_barrier *);
int veo_barrier_destroy(struct veo_barrier *);

int64_t veo_context_tid(struct veo_thr_ctxt *ctx);
int veo_sync_context(struct veo_thr_ctxt *ctx);

#ifdef __cplusplus
} // extern "C"
#endif
#endif
