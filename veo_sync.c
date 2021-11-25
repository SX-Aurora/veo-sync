//
// gcc --shared -fPIC -o libveo_sync.so veo_sync.c -I/opt/nec/ve/veos/include -I. -pthread -L/opt/nec/ve/veos/lib64 -Wl,-rpath=/opt/nec/ve/veos/lib64 -lveo
//
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <errno.h>
#include <ve_offload.h>
#include "veo_sync.h"

#define MAX(a,b) (a >= b ? a : b)

/*
 * barrier helper function, submitted as VH function to a context queue
 */
static uint64_t _veo_barrier_func(void *data)
{
	int rc = -1;
	struct _veo_barrier_args *ba = (struct _veo_barrier_args *)data;

	while (rc == -1)
		rc = __atomic_fetch_add(ba->num_blocked, 1, __ATOMIC_SEQ_CST);
	rc = pthread_barrier_wait(ba->barrier);
	if (rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD)
		return 0;
	return (uint64_t)rc;
}

/*
 * VH function returning the thread ID of a context thread
 */
static uint64_t _veo_get_ctxt_tid(void *data)
{
	return (uint64_t)syscall(SYS_gettid);
}

/*
 * Helper function that collects rc and result from each
 * barrier request.
 */
static int _veo_barrier_collect(struct veo_barrier *vb)
{
	int i, rc, rcmax = 0;

	for (i = 0; i < vb->num_ctxts; i++) {
		rc = veo_call_wait_result(vb->rr[i].ctx,
					  vb->rr[i].req,
					  &vb->rr[i].result);
		rcmax = MAX(rcmax, rc);
		vb->rr[i].rc = rc;
	}
	return rcmax;

}

/**
 * @brief Create a barrier that can be used for synchronizing a set of veo thread
 *        contexts.
 *
 * @param ctxts array of veo_thr_ctxt pointers
 * @param num_ctxts number of contexts
 * @param blocking if nonzero, barrier will block
 * @return pointer to veo_barrier struct
 * @retval NULL if veo_barrier creation failed
 */
struct veo_barrier *veo_barrier_create(struct veo_thr_ctxt **ctxts,
				       int num_ctxts, int blocking)
{
	int i, rc;
	struct veo_barrier *vb = NULL;

	if (num_ctxts <= 0) {
		printf("ERROR: %s num_ctxts must be > 0", __func__);
		goto out_err;
	}
	vb = (struct veo_barrier *)calloc(sizeof(struct veo_barrier), 1);
	if (!vb)
		goto out_err;
	vb->num_ctxts = num_ctxts;
	vb->is_block = blocking;
	vb->num_blocked = 0;
	vb->rr = (struct _veo_req_res *)calloc(num_ctxts, sizeof(struct _veo_req_res));
	if (!vb->rr)
		goto out_err;
	for (i = 0; i < num_ctxts; i++) {
		vb->rr[i].ctx = ctxts[i];
                vb->rr[i].req = VEO_REQUEST_ID_INVALID;
        }
	vb->ba.barrier = &vb->pb;
	vb->ba.num_blocked = &vb->num_blocked;
	return vb;

out_err:
	if (vb) {
		if (vb->rr)
			free(vb->rr);
		free(vb);
	}
	return NULL;
}

/**
 * @brief Activate a barrier across all contexts listed in the veo_barrier.
 *        The contexts will wait until all have reached the barrier, if the
 *        barrier is blocking, they will wait to be unblocked by
 *        veo_barrier_unblock(), otherwise they will just proceed from there.
 *
 * @param vb pointer to veo_barrier struct
 * @return zero all went well
 * @retval nonzero if pthread_barrier_init failed
 */
int veo_barrier_set(struct veo_barrier *vb)
{
	int i, rc;

	rc = pthread_barrier_init(&vb->pb, NULL,
				  (unsigned)(vb->is_block ?
					     vb->num_ctxts + 1 : vb->num_ctxts));
	if (rc != 0) {
		printf("ERROR: pthread_barrier_init returned %d", rc);
		return rc;
	}

	for (i = 0; i < vb->num_ctxts; i++) {
		uint64_t req = veo_call_async_vh(vb->rr[i].ctx, &_veo_barrier_func,
						 &vb->ba);
		// TODO: what if some submission fails?
		if (req == VEO_REQUEST_ID_INVALID) {
			printf("ERROR: submitting request in veo_barrier_set failed!\n");
			return -EINVAL;
		}
		vb->rr[i].req = req;
	}
	return 0;
}

/**
 * @brief Wait until all contexts have reached the barrier. For non-blocking
 *        barrier: collect the return values and return their maximum. Blocking
 *        barriers must collect the return codes when unblocked and will not
 *        make progress before unblocked.
 *
 * @param vb pointer to veo_barrier struct
 * @return zero all went well
 * @retval nonzero if any of the barrier functions returned an error
 */
int veo_barrier_wait(struct veo_barrier *vb)
{
	int i, rc;

	if (vb->is_block) {
		// make sure every context has reached the barrier request
		rc = 0;
		for (;;) {
			__atomic_load(&vb->num_blocked, &rc, __ATOMIC_SEQ_CST);
			if (rc == vb->num_ctxts)
				break;
			usleep(100);
		}
		return 0;
	} else {
		rc = _veo_barrier_collect(vb);
	}
	return rc;
}

/**
 * @brief Release a blocking barrier across all contexts created with
 *        veo_barrier_block(). This function only proceeds when all contexts
 *        have reached the barrier point.
 *
 * @param vb pointer to veo_barrier struct
 * @return zero all went well
 * @retval nonzero if any of the barrier functions returned an error
 */
int veo_barrier_unblock(struct veo_barrier *vb)
{
	int rc;

	if (!vb->is_block)
		return 0;
	
	rc = pthread_barrier_wait(&vb->pb);
	if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
		printf("ERROR: pthread_barrier_wait in unblock returned %d\n", rc);
		return -rc;
	}
	__atomic_store_4(&vb->num_blocked, 0, __ATOMIC_SEQ_CST);
	return _veo_barrier_collect(vb);
}

/**
 * @brief Destroy a veo_barrier, i.e. free its content. This should not be
 *        called when the barrier is being waited for or blocked!
 *
 * @param vb pointer to veo_barrier struct
 * @return zero all went well
 */
int veo_barrier_destroy(struct veo_barrier *vb)
{
	// TODO: handle case where barrier is not unblocked

	int rc = pthread_barrier_destroy(&vb->pb);
	if (rc)
		return rc;
	if (vb->rr)
		free(vb->rr);
	free(vb);
	return 0;
}

/**
 * @brief Return TID of context thread.
 *
 * @param ctx
 * @return TID of context thread
 *
 */
int64_t veo_context_tid(struct veo_thr_ctxt *ctx)
{
	int rc;
	uint64_t req;
	int64_t result;

	req = veo_call_async_vh(ctx, &_veo_get_ctxt_tid, NULL);
	if (req == VEO_REQUEST_ID_INVALID) {
		printf("ERROR: failed to submit request in %s\n", __func__);
		return -EINVAL;
	}
	rc = veo_call_wait_result(ctx, req, &result);
	return result;
}

/**
 * @brief Collect all results from a context, ignore their value and
 *        return the worst return code.
 *
 *        The function collects results until
 *        it encounters a VEO_COMMAND_ERROR which means that it did not
 *        find the request.
 *
 * @param ctx
 *
 */
int veo_sync_context(struct veo_thr_ctxt *ctx)
{
	int rc, rcmax = 0;
	uint64_t result, req;

	req = veo_call_async_vh(ctx, &_veo_get_ctxt_tid, NULL);
	if (req == VEO_REQUEST_ID_INVALID) {
		printf("ERROR: failed to submit request in %s\n", __func__);
		return -EINVAL;
	}
	rcmax = veo_call_wait_result(ctx, req, &result);
	while ((int64_t)--req >= 0) {
		rc = veo_call_wait_result(ctx, req, &result);
		if (rc == VEO_COMMAND_ERROR)
			break;
		rcmax = MAX(rcmax, rc);
	}
	return rcmax;
}
