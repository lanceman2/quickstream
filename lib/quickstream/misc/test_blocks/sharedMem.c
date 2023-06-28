#include <stdio.h>
#include <pthread.h>

#include "../../../../include/quickstream.h"
#include "../../../../lib/debug.h"


// TODO: Since the calling of declare() is serialized, this is not a very
// rigorous test.  We could make other threads that play with the x
// memory; and use the returned mutex to limit access to this
// qsGetMemory() allocated memory.  But the thing I'm testing is not
// whither of not a fucking mutex works, it's whither the interface works
// at all.  I know how to use a fucking mutex between "unrelated" threads
// to control common memory access, without memory corruption.  Most
// people don't know how to do that.  Note: "unrelated" is in the sense
// that threads that run the blocks are not otherwise synchronized by
// other means at the time that they access the memory of interest.
//
// Note: qsGetMemory() is not intended to be a common quickstream
// inter-thread shared memory tool.  It's intended for building very
// complicated block to block interfaces that are not seen by block users.
// So ya, not like control parameters and streams which are known about
// and seen at the highest user level.  It's an internal block building
// tool that makes connections between blocks that know each other very
// well, like for example GTK3 widget blocks: gtk3/button.so,
// gtk3/slider.so, and gtk3/base.so.  They needed to share data that was
// related to widget internals.  Seeing as the core (not sure, maybe just
// peon GTK3 developer) GTK3 developers said what I'm doing is impossible,
// it's interesting.
//
int declare(void) {

    pthread_mutex_t *mutex;
    bool imFirst;

    double *x = qsGetMemory("inter-thread_shared_memory_NAME",
            sizeof(*x), &mutex, &imFirst, 0);

    if(imFirst) {
        // Since this thread is first it has the mutex lock now.  The
        // memory should be zero, via calloc(3) in qsGetMemory().
        fprintf(stderr, "  -- First thread value = %lg\n", *x);
        CHECK(pthread_mutex_unlock(mutex));
    } else {
        // This thread is not first, so it needs to call mutex lock now.
        CHECK(pthread_mutex_lock(mutex));
        *x += 1.0;
        fprintf(stderr, "  -- Other than first thread value = %lg\n",
                *x);
        CHECK(pthread_mutex_unlock(mutex));
    }

    return 0; // success
}


