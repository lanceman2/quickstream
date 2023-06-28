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
#include "mmapRingBuffer.h"



static void
DequeueBlockStreamJobs(struct QsBlock *b) {

    if(b->type == QsBlockType_simple &&
            ((struct QsSimpleBlock *) b)->streamJob) {
        struct QsStreamJob *sj =
                ((struct QsSimpleBlock *) b)->streamJob;
        // Make sure that the stream job is not queued.
        CheckDequeueJob((void *) b, (void *) sj);
        // Reset the some parts of the stream job for the next stream
        // run.
        sj->isFinished = false;
        sj->busy = false;
        sj->didIOAdvance = false;
        sj->lastAvailableCount = 0;
    }

    if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(b = p->firstChild; b; b = b->nextSibling)
            DequeueBlockStreamJobs(b);
    }
}


// The stream job can be 0.   A block can have a stop()
// callback function without having any inputs or outputs.
//
// An error from a block stop callback is handled here.
//
static inline bool
CallBlockStop(struct QsSimpleBlock *b, struct QsStreamJob *j) {

    DASSERT(b);

    if(!b->started)
        // start() for this block was not called in the last cycle; it
        // must have had a block before this one failed in start().
        return true;

    // reset started flag.
    b->started = false;

    if(b->donotFinish) {
        // reset for next start/stop stream sequence
        b->donotFinish = false;
        return true;
    }

    void *dlhandle = GetRunFileDlhandle(b);

    if(!dlhandle && b->runFile) {
        ERROR("Simple block \"%s\" failed to stop",
                b->jobsBlock.block.name);
        return true; // true => fail
    }

    if(b->stop) {
        // The stop() callback function pointer would have been gotten in
        // qsGraph_start().
        int ret;
        // This code is reentrant.  In this case it does not need to be.
        struct QsWhichBlock stackSave;
        SetBlockCallback((void *) b, CB_STOP, &stackSave);
        uint32_t numInputs = 0;
        uint32_t numOutputs = 0;
        if(b->streamJob) {
            numInputs = b->streamJob->numInputs;
            numOutputs = b->streamJob->numOutputs;
        }
        ret = b->stop(numInputs, numOutputs,
                b->jobsBlock.block.userData);
        RestoreBlockCallback(&stackSave);
        if(ret <= -1)
            ERROR("Block \"%s\" stop() failed (ret=%d)",
                    b->jobsBlock.block.name, ret);
        else if(ret >= 1)
            INFO("Block \"%s\" stop() returned %d",
                    b->jobsBlock.block.name, ret);
        if(ret < 0)
            return true; // true => fail
    }

    return false; // false => success
}


// TODO: Figure out what to do about block stop() callback failure.
//
static
bool Stop(struct QsBlock *b) {

    bool ret = false;

    // A block cannot be a simple block and be a parent block.
    DASSERT(!(b->type == QsBlockType_simple &&
            (b->type & QS_TYPE_PARENT)));

    if(b->type == QsBlockType_simple)
        if(CallBlockStop((void *) b,
                    ((struct QsSimpleBlock *) b)->streamJob))
            ret = true;

    if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(b = p->firstChild; b; b = b->nextSibling)
            if(Stop(b))
                ret = true;
    }

    return ret; // false => success
}


static inline
void DestroyStreamArgs(struct QsStreamJob *j) {

    DASSERT(j);

    if(j->numInputs) {

        DASSERT(j->inputs);
        DASSERT(j->inputBuffers);
        DASSERT(j->inputLens);
        DASSERT(j->advanceInputs);

        DZMEM(j->inputBuffers,  j->numInputs*sizeof(*j->inputBuffers));
        DZMEM(j->inputLens,     j->numInputs*sizeof(*j->inputLens));
        DZMEM(j->advanceInputs, j->numInputs*sizeof(*j->advanceInputs));
        free(j->inputBuffers);
        free(j->inputLens);
        free(j->advanceInputs);

        j->inputBuffers = 0;
        j->inputLens = 0;
        j->advanceInputs = 0;
    }

    DASSERT(!j->inputBuffers);
    DASSERT(!j->inputLens);
    DASSERT(!j->advanceInputs);


    if(j->numOutputs) {

        DASSERT(j->outputs);
        DASSERT(j->outputBuffers);
        DASSERT(j->outputLens);
        DASSERT(j->advanceOutputs);

        DZMEM(j->outputBuffers,  j->numOutputs*sizeof(*j->outputBuffers));
        DZMEM(j->outputLens,     j->numOutputs*sizeof(*j->outputLens));
        DZMEM(j->advanceOutputs, j->numOutputs*sizeof(*j->advanceOutputs));
        free(j->outputBuffers);
        free(j->outputLens);
        free(j->advanceOutputs);

        j->outputBuffers = 0;
        j->outputLens = 0;
        j->advanceOutputs = 0;
    }

    DASSERT(!j->outputBuffers);
    DASSERT(!j->outputLens);
    DASSERT(!j->advanceOutputs);
}


static
void DestroyBlockStreamArgs(struct QsBlock *b) {

    if(b->type == QsBlockType_simple) {
        struct QsSimpleBlock *sb = (void *) b;
        sb->donotFinish = 0;
        sb->started = 0;
        if(sb->streamJob)
            DestroyStreamArgs(sb->streamJob);
    } else if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(b = p->firstChild; b; b = b->nextSibling)
            DestroyBlockStreamArgs(b);
    }
}


// This also unlinks the output "pass-throughs".
//
static inline
void DestroyOutputRingBuffer(struct QsOutput *out,
        uint32_t totalNumInputs) {

    DASSERT(out);
    DASSERT(totalNumInputs);

    if(!out->inputs) {
        DASSERT(!out->buffer);
        // Not connected.
        return;
    }

    DASSERT(totalNumInputs);

    if(!out->buffer) {
        DASSERT(!out->prev);
        DASSERT(!out->next);
        // This had a pass through buffer that was cleaned up already.
        // out->passThrough should be set, unless it was changed while the
        // stream was running; so we cannot DASSERT(out->passThrough)
        // here.  You see out->passThrough is not accessed while the
        // stream is running, so we can just set it while the stream is
        // running for the next stream run.
        return;
    }


    // Go to the starting pass-through output.  It's the one with
    // out->prev == 0 or we have a loop.
    //
    // There could be a loop so we need to count; and if there is a loop
    // out->prev will never be zero.  The most we can while() loop is
    // equal to the number of inputs in the whole graph.
    //
    uint32_t loopCount = totalNumInputs + 1;

    while(out->prev) {
        // It must be doubly linked.
        DASSERT(out->prev->next == out);
        // In order for there to be a pass-through buffer created there
        // must be a connection to inputs in this output, out.
        DASSERT(out->inputs);
        DASSERT(out->buffer);
        out = out->prev;
        if(!(--loopCount))
            // We have a loop in the pass through line.
            break;
    }
    // The top output in the line of pass-through buffers is
    // now "out"; unless this is a loop.

    struct QsBuffer *b = out->buffer;
    DASSERT(b);

    freeRingBuffer(b->end - b->mapLength, b->mapLength,
            b->overhangLength);

    DZMEM(b, sizeof(*b));
    free(b);

    // Unset the ring buffer for all outputs that share it.
    while(out) {
        DASSERT(out->buffer == b);
        out->buffer = 0;
        struct QsOutput *next = out->next;
        // Unlink the pass through.  It gets reconnected on the next
        // qsGraph_start(); so that blocks can change there pass-through
        // state between stream runs.
        if(out->prev) {
            // There was a loop.  Remove connections that are behind this
            // output, "out".  This will only happen once in this while()
            // loop, at most.
            DASSERT(out->prev->next == out);
            out->prev->next = 0;
            out->prev = 0;
        }
        if(next) {
            // Remove forward pass through linked list connections.
            DASSERT(next->prev == out);
            next->prev = 0;
            out->next = 0;
        }
        // Go to the next output that is now not linked back.
        out = next;
    }
}



static
void DestroyBlockRingBuffers(struct QsBlock *b) {


    if(b->type == QsBlockType_simple) {
        struct QsSimpleBlock *sb = (void *) b;
        if(sb->streamJob) {
            struct QsStreamJob *sj = sb->streamJob;

            for(uint32_t i = sj->numOutputs - 1; i != -1; --i)
                DestroyOutputRingBuffer(sj->outputs + i,
                        b->graph->numInputs);
        }
    } else if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(b = p->firstChild; b; b = b->nextSibling)
            DestroyBlockRingBuffers(b);
    }
}


int qsGraph_stop(struct QsGraph *g) {

    NotWorkerThread();

    DASSERT(g);

    int ret = 0;

    CHECK(pthread_mutex_lock(&g->mutex));

    if(!g->runningStreams) {
        ret = 1;
        goto finish;
    }

    DSPEW("Stopping graph \"%s\" stream", g->name);

    g->runningStreams = false;

    // Halt just the stream job blocks and their peers.
    //
    uint32_t numHalts = HaltStreamBlocks((void *) g);

    g->streamJobCount = 0;

    DequeueBlockStreamJobs((void *) g);

    Stop((void *) g);

    DestroyBlockStreamArgs((void *) g);

    DestroyBlockRingBuffers((void *) g);

    // We have no pressing need to zero all the input and output counts:
    // QsStreamJob::numOutputs and QsStreamJob::numInputs.  They
    // get counted and set at each start in qsGraph_start().

    g->numInputs = 0;

    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(g);

finish:


    if(g->feedback)
        // Unlike stream start, stream stop never fails; so the state
        // of the stream is stopped.
        g->feedback(g, QsStop, g->fbData);

    CHECK(pthread_mutex_unlock(&g->mutex));

    return ret;
}
