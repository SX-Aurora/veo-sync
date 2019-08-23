//
// gcc -o test_sync test_sync.c -I/opt/nec/ve/veos/include -I. -pthread -L/opt/nec/ve/veos/lib64 -L`pwd` -Wl,-rpath=/opt/nec/ve/veos/lib64 -Wl,-rpath=`pwd` -lveo -lveo_sync
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <ve_offload.h>

#include "veo_sync.h"

#define NUM_CTXT 2

struct veo_proc_handle *proc;
struct veo_thr_ctxt *ctxts[NUM_CTXT];


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
	libh = veo_load_library(proc, "./libvedummy.so");
	if (!libh) {
		printf("veo_load_librray() failed!\n");
		exit(1);
	}

	for (i = 0; i < NUM_CTXT; i++) {
		ctxts[i] = veo_context_open(proc);
		printf("VEO context[%d] = %p\n", i, ctxts[i]);
	}

	for (i = 0; i < NUM_CTXT; i++) {
		argp[i] = veo_args_alloc();
	}
	for (j = 0; j < 30000; j++) {
		for (i = 0; i < NUM_CTXT; i++) {
			uint64_t r = veo_call_async_by_name(ctxts[i], libh, "dummy", argp[i]);
		}
	}
	for (i = 0; i < NUM_CTXT; i++) {
		printf("context %d veo_sync_context() returned %d\n", i,
                       veo_sync_context(ctxts[i]));
	}
	for (i = 0; i < NUM_CTXT; i++) {
		veo_args_free(argp[i]);
	}

	for (i = 0; i < NUM_CTXT; i++) {
		printf("VEO context[%d] = %p, tid = %ld\n", i, ctxts[i],
                       veo_context_tid(ctxts[i]));
	}

	for (i = 0; i < NUM_CTXT; i++) {
		ret = veo_context_close(ctxts[i]);
		printf("close status %d = %d\n", i, ret);
	}
	return 0;
}

