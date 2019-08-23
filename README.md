# VEO Synchronization Primitives

This little library implements tools for synchronizing contexts in
VEO. They need the *VH functions* patched into VEO, i.e. the branch
*feature-vhfunc* from the repository
https://github.com/SX-Aurora/veoffload .


## Building and Usage

Build with `make`.

Test with `make test`

Compile with `-pthread` and link with the `libveo_sync.so` library.

Programming details are below.


## Barriers

A VEO barriers ensures that a set of contexts synchronize and "meet"
at the same time. They are implemented as asynchronous VH functions
which are enqueued into each of the contexts that shall be
synchronized.

Two types of barriers are implemented:
* **Nonblocking barrier**: the contexts wait in the barrier request until all meet at this point. Then they proceed from there with the requests following the barrier request.
* **Blocking barrier**: the contexts wait in the barrier request until all of them reach that point. They do not proceed until the barrier has been unblocked by calling `veo_barrier_unblock()`.


### Creating a barrier

```c
#include <veo_sync.h>

struct veo_barrier *veo_barrier_create(struct veo_thr_ctxt **ctxts,
				       int num_ctxts, int blocking);
```


### Activating the barrier

The following call creates the async VH function requests in each of the contexts:
```c
int veo_barrier_set(struct veo_barrier *b);
```


### Waiting for the barrier

For a nonblocking barrier the following call is making sure that the
contexts have met and passed the barrier. In a blocking barrier the call
ensures that the contexts have met and are waiting at the barrier.
```c
int veo_barrier_wait(struct veo_barrier *b);
```

### Unblocking the barrier

For a blocking barrier the following call is unblocking the contexts
at the barrier, they can proceed with the requests that follow the barrier.
If `veo_barrier_wait()` was not called before, the contexts will certainly
meet at the barrier in this call and proceed.
```c
int veo_barrier_unblock(struct veo_barrier *b);
```
The call is ignored for a nonblocking barrier.


### Destroying the barrier

Finally the barrier related structure is being destroyed:
```c
int veo_barrier_destroy(struct veo_barrier *b);
```
This call should be called only after a `veo_barrier_wait()` (for
nonblocking barriers) or `veo_barrier_unblock()` (for blocking
barriers). If called before the barriers were passed, the behavior
is unpredictable.


### Example: nonblocking barrier
```c
#include <veo_sync.h>

struct veo_thr_ctxt *c[3] = {ctx1, ctx2, ctx3};
struct veo_barrier *b = veo_barrier_create(c, 3, 0);

/* activate the barrier */
rc = veo_barrier_set(b);

/* make sure barriers have met, could have happened before! */
rc = veo_barrier_wait(b);

/* contexts continue here */

veo_barrier_destroy(b);
```


### Example: blocking barrier
```c
#include <veo_sync.h>

struct veo_thr_ctxt *c[3] = {ctx1, ctx2, ctx3};
struct veo_barrier *b = veo_barrier_create(c, 3, 1);

/* activate the barrier */
rc = veo_barrier_set(b);

/* wait until barriers have met */
rc = veo_barrier_wait(b);

/* contexts don't make progress here, still at the barrier */

/* unblock barrier, contexts continue after this */
rc = veo_barrier_unblock(b);

veo_barrier_destroy(b);
```


## Synchronizing a thread context

Collect all results from a context, ignore their value and return the
worst return code.

```c
int veo_sync_context(struct veo_thr_ctxt *ctx);
```

The function collects results until it encounters a VEO_COMMAND_ERROR
which means that it did not find the request. If you do
`veo_call_wait_result()` for some request in the middle of a series of
requests, then `veo_sync_context()` will stop there and not collect
the results of the older request, leaving them in the results queue.

VEO is not able to report which requests are still in the queue and
which not. This call is a little hack to emulate CUDA stream
synchronization, but needs to be used with care. We plan to enhance it
once VEO gets better methods of introspection.


## TID of a context thread

```c
pid_t veo_context_tid(struct veo_thr_ctxt *ctx);
```
