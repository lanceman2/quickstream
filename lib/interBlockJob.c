#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../include/quickstream.h"

#include "debug.h"
#include "Dictionary.h"

#include "c-rbtree.h"
#include "name.h"
#include "threadPool.h"
#include "block.h"
#include "graph.h"
#include "job.h"
#include "port.h"



struct QsInterBlockJob {

    struct QsJob job; // inherit

    // Size of each argument element.
    size_t size;

    void (*callback)(void *data);

    // The number of values available to read in the values buffer.
    //
    // If queueCount exceeds queueLength then we have over-run the
    // queue, and values have been lost.
    //
    uint32_t queueCount;

    // The number of values stored in the ring buffer before it wraps back
    // over itself.  Is the array size of values[] below.
    //
    // values array length.  Like the length of a queue.
    uint32_t queueLength;

    // Points to the next value that will be written in the ring
    // buffer/queue in the "values" array.
    //
    void *w;

    // Points to the next value that will be read in the ring
    // buffer/queue in the "values" array.
    //
    void *r;

    // Queued up data arguments as an array.  Is size queueLength x size.
    void *values;

    // Protects the job and queue.
    pthread_mutex_t mutex;
};



// We need the job lock before calling this.
static inline
void *PopNextTask(struct QsInterBlockJob *j) {

    DASSERT(j);
    DASSERT(j->w);
    DASSERT(j->values);

    if(!j->queueCount) {
        DASSERT(j->r == j->w);
        return 0;
    }

    void *ret = j->r;

    // Go to the next value.
    j->r += j->size;
    if(j->r >= j->values + j->queueLength * j->size)
        j->r = j->values;
    --j->queueCount;

    return ret;
}


static bool
InterBlockWork(struct QsInterBlockJob *j) {

    // We start with the job lock.

    void *v = PopNextTask(j);
    DASSERT(v);

    do {
        uint8_t val[j->size];
        memcpy(val, v, j->size);

        qsJob_unlock(&j->job);

        struct QsWhichBlock stackSave;
        SetBlockCallback((void *) j->job.jobsBlock,
                CB_INTERBLOCK, &stackSave);
        j->callback(val);
        RestoreBlockCallback(&stackSave);

        qsJob_lock(&j->job);

    } while((v = PopNextTask(j)));

    // We return with the job lock.

    return false;
}


static void
FreeInterBlockJob(struct QsInterBlockJob *j) {

    DASSERT(j);
    DASSERT(j->values);
    DASSERT(j->queueLength);

    CHECK(pthread_mutex_destroy(&j->mutex));

    DZMEM(j->values, j->queueLength * j->size);
    free(j->values);
    DZMEM(j, sizeof(*j));
    free(j);
}


struct QsInterBlockJob *
qsAddInterBlockJob(void (*callback)(void *arg), size_t size,
        uint32_t queueLength) {

    NotWorkerThread();
    DASSERT(callback);

    if(!size) size = sizeof(bool);
    if(queueLength < 3) queueLength = 3;

    struct QsSimpleBlock *b = GetBlock(CB_DECLARE,
            0, QsBlockType_simple);

    struct QsInterBlockJob *j = calloc(1, sizeof(*j));
    ASSERT(j, "calloc(1, %zu) failed", sizeof(*j));

    j->values = calloc(queueLength, size);
    ASSERT(j->values, "calloc(%" PRIu32 ",%zu) failed",
            queueLength, size);
    j->w = j->r = j->values;
    j->queueLength = queueLength;
    j->size = size;
    j->callback = callback;

    CHECK(pthread_mutex_init(&j->mutex, 0));

    qsJob_init(&j->job, (void *) b, (void *) InterBlockWork,
            (void *) FreeInterBlockJob, 0);
    qsJob_addMutex(&j->job, &j->mutex);

    return j;
}


// The block makes a wrapper function around this function that other
// blocks use to interface with the block that made this.  The other block
// can include a header file the declares the wrapper.  The blocks using
// the wrapper will be dependent on the block that makes the wrapper.
//
// There may be any number of inter-block wrapper interface functions for
// each QsInterBlockJob.  Each  inter-block wrapper interface function may
// have it's own callback function for each kind of task.
//
void qsQueueInterBlockJob(struct QsInterBlockJob *j, void *data) {

    DASSERT(j);
    ASSERT(data);

    qsJob_lock(&j->job);
    memcpy(j->w, data, j->size);
    // Go to the next value to write to.
    j->w += j->size;
    if(j->w >= j->values + j->queueLength * j->size)
        j->w = j->values;
    
    if(j->queueCount < j->queueLength) {
        ++j->queueCount;
        DASSERT(j->queueCount);
    } else {
        WARN("Block \"%s\" inter-block callback queue was"
                " overrun; we have %" PRIu32 " values",
                j->job.jobsBlock->block.name,
                j->queueLength);
        // Advance the read pointer, because the next read value was
        // overrun and we only have queueLength values to read.
        j->r += j->size;
        if(j->r >= j->values + j->queueLength * j->size)
            j->r = j->values;
    }

    qsJob_queueJob(&j->job);

    qsJob_unlock(&j->job);
}
