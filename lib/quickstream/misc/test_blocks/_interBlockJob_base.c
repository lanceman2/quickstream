// This file adds shared functions and variables to any number of blocks
// that are loaded, but are not shared unless the block load this file
// via: qsOpenRelativeDLHandle(relpath) which is just a small wrapper of
// dlopen(3).

// TODO: this will not work with more than one graph.

#include "string.h"
#include "stdlib.h"
#include "pthread.h"

#include "../../../../include/quickstream.h"
#include "../../../debug.h"

#include "./interBlockJob_base.h"

// The only thing not static is base.
struct Base *base = 0;


// Note: This is called in interBlockJob.so::declare() which will be
// called each time when the interBlockJob.so block is loaded via
// qsGraph_createBlock().  If it was called in block callbacks that run in
// thread pool worker threads we'd have to make this thread-safe.
static
void AddJob(struct QsInterBlockJob *j) {

    ASSERT(j);
    ASSERT(base->numJobs < MAX_JOBS);

    base->jobs[base->numJobs++] = j;

    // This code is kind-of part of the block code libquickstream.so does
    // not know it's not block code; so qsBlockGetName() works okay.
    ERROR("Added job[%" PRIu32 "]=%p from block \"%s\"",
            base->numJobs-1, j, qsBlockGetName());

}

static void __attribute__ ((constructor)) Construct(void);

static void Construct(void) {

    DASSERT(!base);
    base = calloc(1, sizeof(*base));
    ASSERT(base, "calloc(1,%zu) failed", sizeof(*base));

    CHECK(pthread_mutex_init(&base->mutex, 0));

    base->AddJob = AddJob;
}


static void __attribute__ ((destructor)) Destroy(void);

static void Destroy(void) {

    DASSERT(base);

    CHECK(pthread_mutex_destroy(&base->mutex));

#ifdef DEBUG
    memset(base, 0, sizeof(*base));
#endif

    free(base);

    ERROR();
}
