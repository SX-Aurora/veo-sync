//
// gcc -o test_sync test_sync.c -I/opt/nec/ve/veos/include -I. -pthread -L/opt/nec/ve/veos/lib64 -L`pwd` -Wl,-rpath=/opt/nec/ve/veos/lib64 -Wl,-rpath=`pwd` -lveo -lveo_sync
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <ve_offload.h>

#include "veo_sync.h"

#define NUM_CTXT 4

struct veo_proc_handle *proc;
struct veo_thr_ctxt *ctxts[NUM_CTXT];


int test_barrier()
{
	int i, ret;
        struct veo_barrier *vb;

        vb = veo_barrier_create(&ctxts[0], NUM_CTXT, 0);
        ret = veo_barrier_set(vb);
        printf("veo_barrier_set returned %d\n", ret);
        if (ret)
		return 1;

	for (i = 0; i < NUM_CTXT; i++) {
		printf("  vb->rr[%d].req = %lu\n", i, vb->rr[i].req);
	}
	ret = veo_barrier_wait(vb);
        printf("veo_barrier_wait returned %d\n", ret);
	for (i = 0; i < NUM_CTXT; i++) {
		printf("  vb->rr[%d].rc = %d\n", i, vb->rr[i].rc);
	}
	
	ret = veo_barrier_destroy(vb);
        printf("veo_barrier_destroy returned %d\n", ret);

	return 0;
}

int test_barrier_block()
{
	int i, ret;
        struct veo_barrier *vb;

        vb = veo_barrier_create(&ctxts[0], NUM_CTXT, 1);
        printf("created blocking barrier\n");
	ret = veo_barrier_set(vb);
        printf("veo_barrier_set returned %d\n", ret);
        if (ret)
		return 1;
	ret = veo_barrier_wait(vb);
        printf("veo_barrier_wait returned %d\n", ret);
	printf("contexts make no progress here!\n");
	sleep(2);

	ret = veo_barrier_unblock(vb);
        printf("veo_barrier_unblock returned %d\n", ret);
	for (i = 0; i < NUM_CTXT; i++) {
		printf("  vb->rr[%d].rc = %d\n", i, vb->rr[i].rc);
	}
	
	ret = veo_barrier_destroy(vb);
        printf("veo_barrier_destroy returned %d\n", ret);

	return 0;
}


int main()
{
	int i, j, ret;
	uint64_t libh;
        struct veo_barrier *vb;
	struct veo_args *argp[NUM_CTXT];

	proc = veo_proc_create(0);
	if (proc == NULL) {
		printf("veo_proc_create() failed!\n");
		exit(1);
	}

	for (i = 0; i < NUM_CTXT; i++) {
		ctxts[i] = veo_context_open(proc);
		printf("VEO context[%d] = %p\n", i, ctxts[i]);
	}

	if (test_barrier()) {
		printf("test_barrier failed\n");
		return 1;
	}
	
	if (test_barrier_block()) {
		printf("test_barrier_block failed\n");
		return 1;
	}
	
	for (i = 0; i < NUM_CTXT; i++) {
		ret = veo_context_close(ctxts[i]);
		printf("close status %d = %d\n", i, ret);
	}
	return 0;
}

