#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>

#include "../include/quickstream.h"

#include "debug.h"
#include "Dictionary.h"

#include "c-rbtree.h"
#include "name.h"
#include "threadPool.h"
#include "block.h"
#include "graph.h"
#include "job.h"
#include "signalThread.h"



void qsJob_init(struct QsJob *j, struct QsJobsBlock *b,
        bool (*work)(struct QsJob *job),
        void (*destroy)(struct QsJob *j),
        struct QsJob ***peers/*non-zero to share the peers list*/) {

    NotWorkerThread();
    DASSERT(j);
    DASSERT(b);
    // We may pass in blocks that are C casted, so this may catch a bad
    // casting.
    DASSERT(b->block.type & QS_TYPE_JOBS);
    DASSERT(work);

    uint32_t numHalts = qsBlock_threadPoolHaltLock(b);

    memset(j, 0, sizeof(*j));

    j->destroy = destroy;
    j->jobsBlock = b;

    j->mutexes = calloc(1, 2*sizeof(*j->mutexes));
    ASSERT(j->mutexes, "calloc(1,%zu) failed", 2*sizeof(*j->mutexes));
    // j->mutexes[] = [0, 0]
    j->firstMutex = j->mutexes + 1;
    j->lastMutex = j->mutexes + 1;

    j->work = work;


    if(!peers) {
        //
        // Make the empty list of affiliated peer jobs:
        j->peers = calloc(1, sizeof(*j->peers));
        ASSERT(j->peers, "calloc(1,%zu) failed", sizeof(*j->peers));
        // Note: it's null terminated already.
        // (j->peers)[0] = 0
        // j->sharedPeers will stay 0.
    } else {
        // In this case we are sharing the *peers list with all jobs that
        // are in the list.
        j->sharedPeers = peers;
        uint32_t numPeers = 0;

        if(*peers) {
            // Count the jobs in the peers list.
            for(struct QsJob **i = *peers; *i; ++i) {
                // We do not want this new job, j, in this peer list yet.
                DASSERT(*i != j);
                ++numPeers;
            }
        }
        // Append this job, j, to the peers list.
        ++numPeers;
        *peers = realloc(*peers, (numPeers+1)*sizeof(**peers));
        ASSERT(*peers, "realloc(,%zu) failed",
                (numPeers+1)*sizeof(**peers));
        // Add this one.
        (*peers)[numPeers - 1] = j;
        // 0 terminate it.
        (*peers)[numPeers] = 0;
        // j->peers will stay 0.
    }

    // Add this job, j, to the list of all the block's jobs in
    // QsJobsBlock::jobsStack.
    j->n = b->jobsStack;
    if(b->jobsStack)
        b->jobsStack->p = j;
    b->jobsStack = j;
    // j->p is zero already.


    if(peers) {
        DASSERT(*peers);
        // We are already adding peer jobs so:
        //
        // In this case qsJob_addPeers() will not be used.
        //
        // Adjust the block's affiliated thread pools:
        for(struct QsJob **i = *peers; *i; ++i) {
            if(j->jobsBlock == (*i)->jobsBlock) continue;
            Block_addThreadPool(j->jobsBlock, (*i)->jobsBlock->threadPool);
            Block_addThreadPool((*i)->jobsBlock, j->jobsBlock->threadPool);
        }
    }

    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(b->block.graph);
}


// At the time this is called nothing should be accessing this job, j.
// Somehow that must be enforced.
//
void qsJob_addMutex(struct QsJob *j, pthread_mutex_t *mutex) {

    NotWorkerThread();
    DASSERT(j);
    DASSERT(mutex);
    DASSERT(j->mutexes);
    DASSERT(j->jobsBlock);
    DASSERT(j->jobsBlock->block.graph);
    DASSERT((j->peers || j->sharedPeers) &&
            !(j->peers && j->sharedPeers));


    uint32_t numHalts = qsBlock_threadPoolHaltLock(j->jobsBlock);


    // The elements of j->mutexes[] are ordered by increasing mutex address.
    //
    // Look for mutex in the list j->mutexes[]:
    //
    pthread_mutex_t **m = j->firstMutex;
    pthread_mutex_t *save = 0;
    uint32_t numMutexes = 0;
    for(; *m; ++m) {
        if(*m == mutex)
            // We already have it in the list.
            goto finish;
        ++numMutexes;
        if(*m > mutex) {
            // Save this value so that we put is back after:
            save = *m;
            // Put the new mutex here:
            *m = mutex;
            break;
        }
    }
    if(save) {
        for(++m; *m; ++m) {
            ++numMutexes;
            // The addresses must be increasing.
            DASSERT(save < *m);
            pthread_mutex_t *next = *m;
            *m = save;
            save = next;
        }
        // Replace the old 0 terminator with the last "save" one.
        *m = save;
    } else
        // Replace the old 0 terminator with new mutex that happens to
        // have the largest address.
        *m = mutex;

    // Add one to the j->mutexes[] array.
    ++numMutexes;
    DASSERT(numMutexes);
    j->mutexes = realloc(j->mutexes,
            (numMutexes + 2)*sizeof(*j->mutexes));
    ASSERT(j->mutexes, "realloc(,%zu) failed",
            (numMutexes + 2)*sizeof(*j->mutexes));
    // j->mutexes[] = [0, &m1, ..., &mN, 0]
    DASSERT(*j->mutexes == 0);
    j->firstMutex = j->mutexes + 1;
    j->lastMutex = j->mutexes + numMutexes;
    DASSERT(*j->firstMutex);
    DASSERT(*j->lastMutex);
    // This last element we just added (via realloc()) is the new array 0
    // terminator.
    *(j->mutexes + numMutexes + 1) = 0;

finish:

    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(j->jobsBlock->block.graph);
}


// At the time this is called nothing should be accessing this job, j;
// so we need no mutex locks.
//
void qsJob_removeMutex(struct QsJob *j, pthread_mutex_t *mutex) {

    NotWorkerThread();
    DASSERT(j);
    DASSERT(mutex);
    DASSERT(j->mutexes);
    DASSERT(j->jobsBlock);
    DASSERT(j->jobsBlock->block.graph);
    DASSERT((j->peers || j->sharedPeers) &&
            !(j->peers && j->sharedPeers));

    uint32_t numHalts = qsBlock_threadPoolHaltLock(j->jobsBlock);


    // See if we have this mutex to remove.
    pthread_mutex_t **m = j->firstMutex;
    uint32_t numMutexes = 0;
    for(; *m; ++m) {
        if(*m == mutex)
            break;
        ++numMutexes;
    }

    if(!*m)
        // The mutex was not found.
        goto finish;

    // We found it; so shift the array elements after it by one back.
    // The last one will be 0 terminated.
    for(; *m; ++m) {
        ++numMutexes;
        *m = *(m + 1);
    }
    DASSERT(numMutexes);
    // Now shrink the array.  The numMutexes was not incremented for the
    // mutex we are removing, but it was incremented for the 0 terminator.
    --numMutexes;
    // j->mutexes[] = [ 0, &m0, &m1, &m2, ..., 0 ]
    j->mutexes = realloc(j->mutexes,
            (numMutexes + 2)*sizeof(*j->mutexes));
    ASSERT(j->mutexes, "realloc(,%zu) failed",
            (numMutexes + 2)*sizeof(*j->mutexes));
    // j->mutexes[] = [0, &m1, ..., &mN, 0]
    DASSERT(*(j->mutexes + numMutexes + 1) == 0);
    j->firstMutex = j->mutexes + 1;
    if(numMutexes)
        j->lastMutex = j->mutexes + numMutexes;
    else
        // We like to keep j->firstMutex <= j->lastMutex
        j->lastMutex = j->firstMutex;

finish:

    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(j->jobsBlock->block.graph);
}


static inline void
DestroyMutexes(struct QsJob *j) {

    DASSERT(j);
    DASSERT(j->mutexes);
    DASSERT(j->firstMutex > j->mutexes);
    DASSERT(j->lastMutex >= j->firstMutex);

#ifdef DEBUG
    for(pthread_mutex_t **m = j->firstMutex; *m; ++m)
        *m = 0;
#endif

    free(j->mutexes);

#ifdef DEBUG
    j->mutexes = 0;
    j->firstMutex = 0;
    j->lastMutex = 0;
#endif
}


// Adds jB to the list in jA->peers.
//
static inline bool
AddPeer(struct QsJob *jA, struct QsJob *jB) {

    uint32_t numPeers = 0;
    // See if this job, jB is listed already in jA->peers.
    for(struct QsJob **j = jA->peers; *j; ++j) {
        ++numPeers;
        if(*j == jB)
            // It's already there.
            return false;
    }

    // add one peer Job
    ++numPeers;

    jA->peers = realloc(jA->peers,
            (numPeers+1)*sizeof(*jA->peers));
    ASSERT(jA->peers, "realloc(,%zu) failed",
            (numPeers+1)*sizeof(*jA->peers));
    // j->peers[] = [ &j0, &j1, ..., 0 ]
    // It should have been zero terminated from before.
    DASSERT(*(jA->peers + numPeers - 1) == 0);
    *(jA->peers + numPeers - 1) = jB;
    // 0 terminate it.
    *(jA->peers + numPeers) = 0;
    return true;
}


void qsJob_addPeer(struct QsJob *jA, struct QsJob *jB) {

    NotWorkerThread();
    DASSERT(jA);
    DASSERT(jB);
    ASSERT(jA != jB);
    DASSERT(jA->jobsBlock);
    DASSERT(jA->jobsBlock->block.graph);
    DASSERT(jB->jobsBlock);
    DASSERT(jA->peers);
    DASSERT(jB->peers);
    ASSERT(!jA->sharedPeers);
    ASSERT(!jB->sharedPeers);

    DASSERT(jA->jobsBlock->threadPool);
    DASSERT(jB->jobsBlock->threadPool);

    // These lock calls just increase a reference counter if they are
    // already locked; so not a big resource hit:
    //
    uint32_t numHalts = qsBlock_threadPoolHaltLock(jA->jobsBlock);
    if(jA->jobsBlock != jB->jobsBlock)
        numHalts += qsBlock_threadPoolHaltLock(jB->jobsBlock);

    // Add must work or fail the same in both directions:
    //
    // We let the user add one when it is already added; we just do
    // nothing in that case.
    //
    if(AddPeer(jA, jB))
        ASSERT(AddPeer(jB, jA));
    else {
        DASSERT(!AddPeer(jB, jA));
        goto finish;
    }


    if(jA->jobsBlock == jB->jobsBlock)
        // Both jobs are in the same block.
        goto finish;

    // Adjust both block's affiliated thread pools
    Block_addThreadPool(jA->jobsBlock, jB->jobsBlock->threadPool);
    Block_addThreadPool(jB->jobsBlock, jA->jobsBlock->threadPool);

finish:

    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(jA->jobsBlock->block.graph);
}


// Removes jB from the list in jA->peers.
//
static inline bool
RemovePeer(struct QsJob *jA, struct QsJob *jB) {

    uint32_t numPeers = 0;
    // Find the job, jB, in jA->peers.
    struct QsJob **j = jA->peers;
    for(; *j; ++j) {
        if(*j == jB)
            // It's here.
            break;
        ++numPeers;
    }
    if(!*j)
        // It's not in the list.
        return false;

    for(; *j; ++j) {
        ++numPeers;
        *j = *(j + 1);
    }

    DASSERT(numPeers);
    // Decrease the array size.  The one we are removing was not counted
    // yet, but the new terminator was; so we must still do:
    --numPeers;

    jA->peers = realloc(jA->peers, (numPeers+1)*sizeof(*jA->peers));
    ASSERT(jA->peers, "realloc(,%zu) failed",
            (numPeers+1)*sizeof(*jA->peers));
    DASSERT(*(jA->peers + numPeers) == 0);
    return true;
}


void qsJob_removePeer(struct QsJob *jA, struct QsJob *jB) {
    NotWorkerThread();
    DASSERT(jA);
    DASSERT(jB);
    ASSERT(jA != jB);
    DASSERT(jA->jobsBlock);
    DASSERT(jA->jobsBlock->block.graph);
    DASSERT(jB->jobsBlock);
    DASSERT(jA->peers);
    DASSERT(jB->peers);
    ASSERT(!jA->sharedPeers);
    ASSERT(!jB->sharedPeers);

    // These lock calls just increase a reference counter if they are
    // already locked; so not a big resource hit:
    //
    uint32_t numHalts = qsBlock_threadPoolHaltLock(jA->jobsBlock);
    if(jA->jobsBlock != jB->jobsBlock)
        numHalts += qsBlock_threadPoolHaltLock(jB->jobsBlock);


    // Remove must work or fail the same in both directions:
    //
    // We let the user remove one when it is already removed; we just do
    // nothing in that case.
    //
    if(RemovePeer(jA, jB))
        ASSERT(RemovePeer(jB, jA));
    else {
        DASSERT(!RemovePeer(jB, jA));
        goto finish;
    }


    // The rest is just changing the jobs block threadPools list
    // QsJobsBlock::threadPools:

    if(!jA->jobsBlock->threadPool) {
        DASSERT(!jB->jobsBlock->threadPool);
        // This is the case when the qsGraph_destroy() is being called.
        goto finish;
    }
#ifdef DEBUG
    else
        DASSERT(jB->jobsBlock->threadPool);
#endif


    if(jA->jobsBlock == jB->jobsBlock)
        // Both jobs are in the same block.
        goto finish;

    // Adjust both block's affiliated thread pools
    Block_removeThreadPool(jA->jobsBlock, jB->jobsBlock->threadPool);
    Block_removeThreadPool(jB->jobsBlock, jA->jobsBlock->threadPool);

finish:

    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(jA->jobsBlock->block.graph);
}


void qsJob_cleanup(struct QsJob *j) {

    NotWorkerThread();
    DASSERT(j);
    struct QsJobsBlock *b = j->jobsBlock;
    DASSERT(b);
    DASSERT(b->block.graph);
    DASSERT((j->peers || j->sharedPeers) &&
            !(j->peers && j->sharedPeers));


    uint32_t numHalts = qsBlock_threadPoolHaltLock(b);

    DASSERT(b->threadPool);
    DASSERT(j->mutexes);


    // Remove the block from the thread pool's block queue if there are no
    // jobs except this job, j, in the block's job queue:
    if(b->first == j && !j->next) {
        DASSERT(b->inQueue);
        DASSERT(b->last == j);
        DASSERT(!j->prev);
        PullBlock(b->threadPool, b);
    }

    // Remove this job from the block's job queue, if it is in there.
    if(j->inQueue) {
        if(j->next) {
            DASSERT(b->last != j);
            j->next->prev = j->prev;
        } else if(b->last == j)
            b->last = j->prev;

        if(j->prev) {
            DASSERT(b->first != j);
            j->prev->next = j->next;
        } else if(b->first == j)
            b->first = j->next;
    }
#ifdef DEBUG
    else {
        DASSERT(!j->next);
        DASSERT(!j->prev);
        DASSERT(b->first != j);
        DASSERT(b->last != j);
    }
#endif


    DASSERT(b->jobsStack);

    // Remove this job, j, from the list of all the block's jobs in
    // QsJobsBlock::jobsStack of b.
    if(j->p) {
        DASSERT(b->jobsStack != j);
        j->p->n = j->n;
    } else {
        DASSERT(b->jobsStack == j);
        b->jobsStack = j->n;
    }
    if(j->n)
        j->n->p = j->p;


    // Remove all the elements in the j->peer[] array along with
    // those in the peer jobs.
    if(j->peers)
        while(*j->peers)
            qsJob_removePeer(j, *j->peers);


    if(j->sharedPeers) {

        uint32_t numPeers = 0;
        struct QsJob **i;
        for(i = *j->sharedPeers; *i; ++i) {
            ++numPeers;
            if(*i == j)
                break;
            if(j->jobsBlock == (*i)->jobsBlock) continue;
            Block_removeThreadPool(j->jobsBlock,
                    (*i)->jobsBlock->threadPool);
            Block_removeThreadPool((*i)->jobsBlock,
                    j->jobsBlock->threadPool);
        }
        DASSERT(numPeers);
        DASSERT(*i == j);

        for(++i; *i; ++i) {
            ++numPeers;
            DASSERT(*i != j);
            if(j->jobsBlock != (*i)->jobsBlock) {
                Block_removeThreadPool(j->jobsBlock,
                        (*i)->jobsBlock->threadPool);
                Block_removeThreadPool((*i)->jobsBlock,
                        j->jobsBlock->threadPool);
            }
            // Shift the array element back one.
            *(i - 1) = *i;
        }
        DASSERT(numPeers);
        // The new 0 terminator.
        *(i - 1) = 0;

        if(numPeers > 1) {
            // Shrink the array by one.
            //
            // Note: we are using the old numPeers which is numPeers + 1
            // now, given we need an extra null terminator element.
            *j->sharedPeers = realloc(*j->sharedPeers,
                    numPeers * sizeof(**j->sharedPeers));

            ASSERT(*j->sharedPeers, "realloc(,%zu) failed",
                    numPeers * sizeof(**j->sharedPeers));
        } else {
            // numPeers == 1
            //
            // free the array of job pointers.
            free(*j->sharedPeers);

            // The control parameters will use this in
            // parameter.c function MergeGroups()
            *j->sharedPeers = 0;

            // The user owns the j->sharedPeer handle memory, so we do not
            // free it.  They will see that *sharedPeers as passed to
            // qsJob_init() is 0, and the list is empty.
        }
    }


    // Just destroying the pointers to mutexes called QsJob::mutexes; and
    // not the mutexes themselves.
    DestroyMutexes(j);


    if(j->peers)
        free(j->peers);


    if(j->destroy)
        // Call the specific job destructor.
        j->destroy(j);
    else
        memset(j, 0, sizeof(*j));


    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(b->block.graph);
}
