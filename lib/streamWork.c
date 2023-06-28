#include <string.h>
#include <stddef.h>
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
#include "stream.h"



// Like FixFlowArgs() but does not set inputLens[] and outputLens[];
// it just tallies them.
//
// Returns the total of all inputLens[] and outputLens[] that are
// ready to be used by the block; that is if we where to set them.
//
static inline size_t
GetAvailableCount(const struct QsStreamJob *j) {

    size_t availableCount = 0;

    for(uint32_t i = j->numInputs - 1; i != -1; --i) {
        
        // Note: this flow() could be changing j->advanceInputs[i] right
        // now, not like in FixFlowArgs() where the stream job worker
        // thread is the one calling FixFlowArgs(); so we cannot call:
        // DASSERT(j->advanceInputs[i] == 0);

        size_t len;

        if(j->inputs[i].readLength > j->inputs[i].maxRead)
            len = j->inputs[i].maxRead;
        else
            len = j->inputs[i].readLength;

        if(len > j->inputs[i].maxRead)
            len = j->inputs[i].maxRead;

        availableCount += len;
    }


    for(uint32_t i = j->numOutputs - 1; i != -1; --i) {

        // Note: this flow() could be changing j->advanceOutputs[i] right
        // now, not like in FixFlowArgs() where the stream job worker
        // thread is the one calling FixFlowArgs(); so we cannot call:
        // DASSERT(j->advanceOutputs[i] == 0);

        if(j->outputs[i].isFlushing)
            continue;

        size_t maxReadLength = 0;

        struct QsOutput *out = j->outputs + i;
        DASSERT(out->numInputs);

        // Find the largest readLength from all the inputs that this
        // output, out, feeds.
        for(uint32_t k = out->numInputs - 1; k != -1; --k) {
            struct QsInput *in = out->inputs[k];
            if(maxReadLength < in->readLength)
                maxReadLength = in->readLength;
        }

        size_t len = 0;

        if(out->maxWrite + out->maxMaxRead > maxReadLength) {
            len = out->maxWrite + out->maxMaxRead - maxReadLength;
            if(len > out->maxWrite)
                len = out->maxWrite;
        }

        availableCount += len;
    }

    return availableCount;
}


// get j->inputLens[] and j->outputLens[]
//
//
static inline void
FixFlowArgs(struct QsStreamJob *j) {

    for(uint32_t i = j->numInputs - 1; i != -1; --i) {
        DASSERT(j->advanceInputs[i] == 0);

        if(j->inputs[i].readLength > j->inputs[i].maxRead)
            j->inputLens[i] = j->inputs[i].maxRead;
        else
            j->inputLens[i] = j->inputs[i].readLength;

        if(j->inputLens[i] > j->inputs[i].maxRead)
            j->inputLens[i] = j->inputs[i].maxRead;

//ERROR("j->inputLens[%" PRIu32 "] = %zu", i, j->inputLens[i]);
    }


    for(uint32_t i = j->numOutputs - 1; i != -1; --i) {
        DASSERT(j->advanceOutputs[i] == 0);

        if(j->outputs[i].isFlushing) {
            j->outputLens[i] = 0;
            continue;
        }

        size_t maxReadLength = 0;

        struct QsOutput *out = j->outputs + i;
        DASSERT(out->numInputs);

        // Find the largest readLength from all the inputs that this
        // output, out, feeds.
        for(uint32_t k = out->numInputs - 1; k != -1; --k) {
            struct QsInput *in = out->inputs[k];
            if(maxReadLength < in->readLength)
                maxReadLength = in->readLength;
        }

        if(out->maxWrite + out->maxMaxRead > maxReadLength) {
            j->outputLens[i] =
                out->maxWrite + out->maxMaxRead - maxReadLength;
            if(j->outputLens[i] > out->maxWrite)
                j->outputLens[i] = out->maxWrite;
        } else
            j->outputLens[i] = 0;

//ERROR("j->outputLens[%" PRIu32 "] = %zu", i, j->outputLens[i]);
    }
}


static inline void
AdvanceRingBufferpointers(struct QsStreamJob *j) {

    // Here we advance read and write pointers due to this stream job
    // reading and writing.  We also calculate the distance between
    // the write and read pointers (readLength).  Read more comments
    // for a more detailed understanding.
    //

    // Count the total bytes advanced in the last flow() (or flush())
    // call.
    j->lastAvailableCount = 0;

    for(uint32_t i = j->numInputs - 1; i != -1; --i) {
        DASSERT(j->advanceInputs[i] <= j->inputLens[i]);
#ifdef DEBUG
        struct QsInput *in = j->inputs + i;
        DASSERT(j->inputBuffers[i] < in->output->buffer->end);
        DASSERT(j->inputBuffers[i] >= in->output->buffer->end -
                in->output->buffer->mapLength);
#endif

        j->lastAvailableCount += j->inputLens[i];

        // Advance reading Ring Buffer Pointers
        if(j->advanceInputs[i]) {

            struct QsInput *in = j->inputs + i;

            j->inputBuffers[i] += j->advanceInputs[i];
            if(j->inputBuffers[i] >= in->output->buffer->end)
                j->inputBuffers[i] -= in->output->buffer->mapLength;
            //
            // Decrease the input readLength for this input.
            //
            // readLength is the distance between the write pointer and
            // this input's (j->inputs[i]) read pointer, on the ring
            // buffer.  Here we tally the decrease in it due to the
            // advancement of the read pointer.
            //
            in->readLength -= j->advanceInputs[i];
            j->advanceInputs[i] = 0;
            if(!j->didIOAdvance)
                j->didIOAdvance = true;
        }
    }


    for(uint32_t i = j->numOutputs - 1; i != -1; --i) {
        struct QsOutput *out =  j->outputs + i;
        DASSERT(j->advanceOutputs[i] <= j->outputLens[i]);
        DASSERT(j->outputBuffers[i] < out->buffer->end);
        DASSERT(j->outputBuffers[i] >= out->buffer->end -
                out->buffer->mapLength);

        if(!j->outputs[i].isFlushing)
            j->lastAvailableCount += j->outputLens[i];


        if(j->advanceOutputs[i]) {
            // Advance writing Ring Buffer Pointers
            j->outputBuffers[i] += j->advanceOutputs[i];
            if(j->outputBuffers[i] >= out->buffer->end)
                j->outputBuffers[i] -= out->buffer->mapLength;
            //
            // Increase the input read lengths that this output feeds.
            //
            // Note: we change input readLength since we cannot change
            // inputBuffers[] (read pointers) while there may be a
            // different block worker thread using that in a flow() or
            // flush() call without a mutex lock.  In this way readLength
            // is the magic sauce.
            //
            // That's how we can release mutex locks while we call the
            // flow() and flush() block callback functions.
            //
            for(uint32_t k = out->numInputs - 1; k != -1; --k) {
                //
                // readLength is the distance between this input pointer
                // (in) and the write (output) pointer, in the ring
                // buffer.  Here we tally the increases in it due to the
                // advancement of the write pointer.
                //
                struct QsInput *in = out->inputs[k];
                in->readLength += j->advanceOutputs[i];
            }

            j->advanceOutputs[i] = 0;
            if(!j->didIOAdvance)
                j->didIOAdvance = true;
        }
    }
}


// Return true if a simple block can call flow() or flush().
//
static inline bool
CheckStreamJob(const struct QsStreamJob *j) {

    DASSERT(j->numInputs || j->numOutputs);

    if(j->busy || j->isFinished)
        // We'll let the worker thread that is running or did just run
        // flow() or flush() be the one that decides to keep running or
        // not.
        return false;

    size_t availableCount = GetAvailableCount(j);

    if(j->didIOAdvance)
        // If nothing is available then we do not want to call flow() or
        // flush(), but otherwise yes.
        return availableCount;


    // !j->didIOAdvance
    //
    // At this point we know that in the last call to flow() or flush()
    // there were was no stream I/O advancement or output flush state
    // changes.
    //
    // The availableCount can only be increased by other stream jobs.
    //
    DASSERT(availableCount >= j->lastAvailableCount,
            "availableCount(%zu) >= (%zu)j->lastAvailableCount",
            availableCount, j->lastAvailableCount);


    if(j->lastAvailableCount == availableCount)
        return false;

    return true;
}


static inline struct QsSimpleBlock *
GetSimpleBlock(struct QsStreamJob *j) {

    return (void *) (((struct QsJob *)j)->jobsBlock);
}


// Get the first stream job pointer pointer from the job peers in a stream
// job.
//
static inline struct QsStreamJob **
GetStreamJobPP(struct QsStreamJob *j) {

    return (void *) (((struct QsJob *) j)->peers);
}


// Queues work for all neighboring blocks (stream neighbors) that can work
// the stream.
//
// Returns true to have this block, b, continue calling flow() or flush()
// in StreamWork().
//
static inline void
QueueWorkCalls(struct QsStreamJob *sj) {

    // The peer jobs do not include the job that refers to it's peers.
    // Peers are not the same for all peers in the peer groups.  How else
    // can I say it...
    //
    // For control parameter jobs it's different.  All control parameter
    // jobs are in fully connected groups of jobs which include all
    // control parameter jobs that are in a group.  So a control parameter
    // job is a peer to itself and all in it's connect group.

    // For all peer jobs which are stream jobs:
    for(struct QsStreamJob **j = GetStreamJobPP(sj); *j; ++j) {
        DASSERT((*j) != sj);
        if(CheckStreamJob((void *)(*j)))
            // If the stream job is running already this does the right
            // thing.  qsJob_queueJob() will not queue it if the stream
            // job, (*j), is running, and that's what we want.
            qsJob_queueJob((void *) (*j));
    }
}


static inline
void CheckSignalFinish(struct QsGraph *g) {

    DASSERT(g);

    CHECK(pthread_mutex_lock(&g->cqMutex));

    DASSERT(g->streamBlockCount != 0);

    --g->streamBlockCount;

    if(g->streamBlockCount != 0)
        goto finish;

    if(g->waiting)
        CHECK(pthread_cond_signal(&g->cqCond));
    else
        INFO("No master thread waiting for stream to end");

finish:

    CHECK(pthread_mutex_unlock(&g->cqMutex));
}


bool StreamWork(struct QsStreamJob *j) {

    DASSERT(j->flow);

    struct QsSimpleBlock *b = GetSimpleBlock(j);

    DASSERT(!j->isFinished,
            "block \"%s\"", b->jobsBlock.block.name);

//ERROR("                    block \"%s\"", b->jobsBlock.block.name);

    FixFlowArgs(j);


    // Reinitialize the didIOAdvance flag.  It will be set if there was
    // any flow action (flow in or out) by this stream job in the next
    // flow() or flush() call.
    j->didIOAdvance = false;
    DASSERT(!j->busy);
    j->busy = true;

    //////////////////////////////////////////
    qsJob_unlock((void *) j);

    struct QsWhichBlock stackSave;
    SetBlockCallback((void *) b, CB_FLOW, &stackSave);

    // TODO: Call flow() or flush().
    //
    int workRet = j->flow((const void * const *) j->inputBuffers,
            j->inputLens, j->numInputs,
            j->outputBuffers, j->outputLens, j->numOutputs,
            b->jobsBlock.block.userData);

    RestoreBlockCallback(&stackSave);

    qsJob_lock((void *) j);
    //////////////////////////////////////////

    DASSERT(j->busy);
    j->busy = false;

    AdvanceRingBufferpointers(j);

    QueueWorkCalls(j);

    if(workRet) {
        DASSERT(!j->isFinished);
        INFO("Flushing source block \"%s\" (%" PRIu32 ") outputs",
                b->jobsBlock.block.name, j->numOutputs);
        j->isFinished = true;
        CheckSignalFinish(b->jobsBlock.block.graph);
        DASSERT(!j->job.inQueue);
        return false;
    }

    return CheckStreamJob(j); // false == stop calling for now.
}


void CheckStreamFinished(struct QsGraph *g) {

    DASSERT(!g->streamJobCount);

    CHECK(pthread_mutex_lock(&g->cqMutex));

    DSPEW("Graph \"%s\" stream finished running, no stream jobs",
            g->name);

    if(g->waiting)
        CHECK(pthread_cond_signal(&g->cqCond));
    else
        INFO("No master thread waiting for stream to end");

    CHECK(pthread_mutex_unlock(&g->cqMutex));
}
