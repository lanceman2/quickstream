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
#include "FindFullPath.h"
#include "stream.h"
#include "mmapRingBuffer.h"



// We had a problem that the stream would start when there where no blocks
// with connected stream ports.  Now we call this from qsGraph_start() to
// check.
static inline
bool CheckForConnectedStreams(struct QsBlock *b) {

    if(b->type & QS_TYPE_PARENT) {
        // This could be a super block or the top block graph.
        struct QsParentBlock *p = (void *) b;
        // We reuse variable b.  No reason not to.
        // It does not get used after this for loop.
        for(b = p->firstChild; b; b = b->nextSibling)
            if(CheckForConnectedStreams(b))
                return true;
    } else if(b->type == QsBlockType_simple &&
        ((struct QsSimpleBlock *) b)->streamJob) {
        struct QsStreamJob *j = ((struct QsSimpleBlock *) b)->streamJob;
        DASSERT((j->maxOutputs && j->outputs) ||
                (!j->maxOutputs && !j->outputs));
        for(int i = j->maxOutputs-1; i != -1; --i) {
            // Note: we only check for a connection to a stream output.
            // If there is a connection to a stream output than there must
            // be a connection to a stream input.
            if(j->outputs[i].inputs) {
                DASSERT(j->outputs[i].numInputs);
                // We have a connection to output port i.
                // We just needed to find one, so we are done.
                return true;
            }
        }
    }
    // No connection found in this block, b.
    return false;
}


// Halt just the job blocks with a stream job.
//
uint32_t HaltStreamBlocks(struct QsBlock *b) {

    uint32_t haltCount = 0;

    // The halted blocks are only simple blocks.  Only simple blocks have
    // stream jobs.
    if(b->type == QsBlockType_simple &&
            ((struct QsSimpleBlock *) b)->streamJob)
        haltCount += qsBlock_threadPoolHaltLock((void *) b);

    if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(struct QsBlock *b = p->firstChild; b; b = b->nextSibling)
            haltCount += HaltStreamBlocks(b);
    }

    return haltCount;
}


// Return true if there is a missing connection that is required.
//
// Also calculate j->numOutputs and j->numInputs if saveNums is set.
//
static inline
bool CheckForStreamGap(struct QsStreamJob *j, bool saveNums) {

    DASSERT(j);

    // These two counts may be set from a failed start:
    j->numInputs = 0;
    j->numOutputs = 0;


    // The stream for this graph should not be setup yet.
    DASSERT(!j->inputBuffers);
    DASSERT(!j->inputLens);
    DASSERT(!j->advanceInputs);

    DASSERT(!j->outputBuffers);
    DASSERT(!j->outputLens);
    DASSERT(!j->advanceOutputs);


    // Count outputs that are connected and check for gaps in output
    // connections.
    uint32_t num = j->minOutputs;
    uint32_t i = 0;
    for(; i < num; ++i)
        if(!j->outputs[i].inputs) {
            WARN("Block \"%s\" stream output port %s not connected",
                    j->outputs[i].port.block->name,
                    j->outputs[i].port.name);
            return true;
        }
    uint32_t max = j->maxOutputs;
    for(; i < max; ++i)
        if(j->outputs[i].inputs) {
            DASSERT(j->outputs[i].numInputs);
            // We have a connection to port i
            if(num != i) {
                // We have no connection to port i-1.
                WARN("Block \"%s\" stream output port %s gap not connected",
                        j->outputs[i-1].port.block->name,
                        j->outputs[i-1].port.name);
                return true;
            }
            ++num;
            continue;
        }
    if(saveNums)
        j->numOutputs = num;


    // Count inputs that are connected and check for gaps in input
    // connections.
    num = j->minInputs;
    i = 0;
    for(; i < num; ++i)
        if(!j->inputs[i].output) {
            WARN("Block \"%s\" stream input port %s not connected",
                    j->inputs[i].port.block->name,
                    j->inputs[i].port.name);
            goto fail;
        }
    max = j->maxInputs;
    for(; i < max; ++i) {
        if(j->inputs[i].output) {
            // We have a connection to port i
            if(num != i) {
                // We have no connection to port i-1.
                WARN("Block \"%s\" stream input port %s gap not connected",
                        j->inputs[i-1].port.block->name,
                        j->inputs[i-1].port.name);
                goto fail;
            }
            ++num;
            continue;
        }
    }

    if(saveNums)
        j->numInputs = num;


    return false; // false is success.

fail:

    if(saveNums)
        j->numOutputs = 0;

    return true; // true is failure.
}


// Returns true is there is a gap in the stream connection ports.
//
// Also tallies up the total number of stream inputs that are connected.
static
bool CheckForStreamGaps(struct QsBlock *b, uint32_t *numInputs) {

    bool ret = false;

    if(b->type == QsBlockType_simple) {
        struct QsStreamJob *j = ((struct QsSimpleBlock *) b)->streamJob;
        if(j) {
            if(CheckForStreamGap(j, true))
                ret = true; // we have a gap
            *numInputs += j->numInputs;
        }
    }

    if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(struct QsBlock *b = p->firstChild; b; b = b->nextSibling)
            if(CheckForStreamGaps(b, numInputs))
                ret = true;
    }

    return ret;
}


// Return false if we can run the graph.
//
// We need to have a graph mutex before calling this.
bool
CheckStreamConnections(struct QsGraph *g, uint32_t *numInputs) {

    ASSERT(numInputs);

    if(g->runningStreams)
        // We are running the stream already.
        return false;


    // We cannot have connection gaps in the stream ports.
    return CheckForStreamGaps((void *) g, numInputs);
}


static inline void
CreateBlockPassThrough(struct QsStreamJob *j) {

    for(uint32_t i = j->numInputs - 1; i != -1; --i) {
        struct QsInput *in = j->inputs + i;
        // If the pass through input and output ports are connected make
        // the output doubly linked list link.
        if(in->passThrough && in->output &&
                in->passThrough->numInputs) {
            struct QsOutput *out = in->passThrough;
            struct QsOutput *prev = in->output; // feed output
            DASSERT(!out->prev);

#ifdef DEBUG
            if(prev->next)
                ASSERT(prev->next->passThrough);
#endif

            // TODO: Add some failure modes to catch this before here
            // like in qsGraph_connect() and qsMakePassThroughBuffer().
            // Also add more test programs in ../tests/ that test this.
            // 
            // We decided that it does not make good sense to have pass
            // through stream buffers with forks.  The two inputs that
            // receive the data end up both writing to the same buffer,
            // meaning that the block would have too "know" the structure
            // of the flow graph, adding a not to useful abstraction as to
            // how inputs are related.  The result would not be much more
            // than a fork at an output to two (or more) inputs of a
            // non-pass-through.
            //
            // We can't have an output connected to two pass through
            // inputs.  We need ASSERT() here not DASSERT():
            ASSERT(!prev->next, "You cannot have an output connected to "
                    "two pass through inputs: block \"%s\" output \"%s\" "
                    "is connected to  block \"%s\" input \"%s\" and "
                    "block \"%s\" input \"%s\"",
                    prev->port.block->name, prev->port.name,
                    in->port.block->name, in->port.name,
                    prev->next->passThrough->port.block->name,
                    prev->next->passThrough->port.name);
            out->prev = prev;
            prev->next = out;
        }
    }
}


static void
CreateBlockPassThroughs(struct QsBlock *b) {

    if(b->type == QsBlockType_simple) {
        struct QsSimpleBlock *sb = (void *) b;
        if(sb->streamJob) 
            CreateBlockPassThrough(sb->streamJob);
    } else if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(b = p->firstChild; b; b = b->nextSibling)
            CreateBlockPassThroughs(b);
    }
}


static inline
void CreateOutputRingBuffer(struct QsOutput *out) {

    DASSERT(out);

    if(!out->inputs) {
        DASSERT(!out->numInputs);
        // Not connected.
        return;
    }
    DASSERT(out->numInputs);

    if(out->buffer) return;


    struct QsOutput *o = out;

    // Go to the starting pass-through output.  It's the one with
    // out->prev == 0.
    while(out->prev) {
        // It must be doubly linked.
        DASSERT(out->prev->next == out);
        // In order for there to be a pass-through buffer created there
        // must be a connection to inputs in this output, out.
        DASSERT(out->numInputs);
        DASSERT(out->inputs);
        DASSERT(!out->buffer);
        out = out->prev;

        ASSERT(out != o, // TODO: ya this.
                "There's a loop in the pass-through ring buffer."
                " Write code in qsBlock_connectStreams() to return "
                "with error in this case");
    }
    // The top output in the line of pass-through buffers.
    o = out;

    size_t len = 0;
    size_t overhangLen = 0;

    while(out) {
        DASSERT(out->nextMaxWrite);
        DASSERT(out->maxWrite);
        if(out->maxWrite != out->nextMaxWrite)
            // The block changed this setting before this stream start
            // (before now).
            out->maxWrite = out->nextMaxWrite;
        out->maxMaxRead = 0;
        for(uint32_t i = out->numInputs - 1; i != -i; --i) {
            struct QsInput *in = *(out->inputs + i);
            DASSERT(in->maxRead);
            DASSERT(in->nextMaxRead);
            if(in->maxRead != in->nextMaxRead)
                in->maxRead = in->nextMaxRead;

            if(out->maxMaxRead < in->maxRead )
               out->maxMaxRead = in->maxRead;
        }
        len += out->maxMaxRead + out->maxWrite;
        // The overhang length is the largest read or write possible
        // to the ring buffer.
        if(overhangLen < out->maxMaxRead)
            overhangLen = out->maxMaxRead;
        if(overhangLen < out->maxWrite)
            overhangLen = out->maxWrite;

        out = out->next;
        // For a non-pass-through buffer out will be zero.
    }

    // Now we have the ring buffer memory mapping lengths:
    // len and overhangLen.

    // Reset to top output.
    out = o;

    struct QsBuffer *b = calloc(1, sizeof(*b));
    ASSERT(b, "calloc(1,%zu) failed", sizeof(*b));

    // This will ASSERT if it fails.
    void *start = makeRingBuffer(&len, &overhangLen);

    b->end = start + len;
    b->mapLength = len;
    b->overhangLength = overhangLen;

    // Set the ring buffer for all outputs that share it.
    while(out) {
        out->buffer = b;
        out = out->next;
    }
}


// This assumes there are no gaps in the stream connected ports in
// either input ports or output ports.
static
void CreateBlockRingBuffers(struct QsBlock *b) {

    if(b->type == QsBlockType_simple) {
        struct QsSimpleBlock *sb = (void *) b;
        if(sb->streamJob) {
            struct QsStreamJob *sj = sb->streamJob;
            for(uint32_t i = sj->numOutputs - 1; i != -1; --i)
                CreateOutputRingBuffer(sj->outputs + i);
        }
    } else if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(b = p->firstChild; b; b = b->nextSibling)
            CreateBlockRingBuffers(b);
    }
}


static inline
void CreateStreamArgs(struct QsStreamJob *j) {

    DASSERT(j);

    DASSERT(!j->inputBuffers);
    DASSERT(!j->inputLens);
    DASSERT(!j->advanceInputs);

    DASSERT(!j->outputBuffers);
    DASSERT(!j->outputLens);
    DASSERT(!j->advanceOutputs);


    if(j->numInputs) {
        DASSERT(j->inputs);
        DASSERT(j->maxInputs);

        j->inputBuffers = calloc(j->numInputs,
                sizeof(*j->inputBuffers));
        ASSERT(j->inputBuffers,
                "calloc(%" PRIu32 ",%zu) failed",
                j->numInputs, sizeof(*j->inputBuffers));
        j->inputLens = calloc(j->numInputs,
                sizeof(*j->inputLens));
        ASSERT(j->inputLens,
                "calloc(%" PRIu32 ",%zu) failed",
                j->numInputs, sizeof(*j->inputLens));
        j->advanceInputs = calloc(j->numInputs,
                sizeof(*j->advanceInputs));
        ASSERT(j->advanceInputs,
                "calloc(%" PRIu32 ",%zu) failed",
                j->numInputs, sizeof(*j->advanceInputs));

        // Set the read pointers for these inputs.
        //
        // That is, initialize the args:
        //
        for(uint32_t i = j->numInputs - 1; i != -1; --i) {
            DASSERT(j->inputs[i].output);
            struct QsBuffer *buffer = j->inputs[i].output->buffer;
            DASSERT(buffer);
            DASSERT(buffer->end);
            DASSERT(buffer->mapLength);
            DASSERT(buffer->mapLength >= buffer->overhangLength);
            DASSERT(buffer->overhangLength);
            // We can initialize the flow() input args:
            //
            // Set the read pointer to the start of the memory
            // mapping.
            j->inputBuffers[i] = buffer->end - buffer->mapLength;
            // j->inputLens[i] already 0
            // j->advanceInputs[i] is 0 already.
            j->inputs[i].readLength = 0;
        }
    }

    if(j->numOutputs) {
        DASSERT(j->outputs);
        DASSERT(j->maxOutputs);

        j->outputBuffers = calloc(j->numOutputs,
                sizeof(*j->outputBuffers));
        ASSERT(j->outputBuffers,
                "calloc(%" PRIu32 ",%zu) failed",
                j->numOutputs, sizeof(*j->outputBuffers));
        j->outputLens = calloc(j->numOutputs,
                sizeof(*j->outputLens));
        ASSERT(j->outputLens,
                "calloc(%" PRIu32 ",%zu) failed",
                j->numOutputs, sizeof(*j->outputLens));
        j->advanceOutputs = calloc(j->numOutputs,
                sizeof(*j->advanceOutputs));
        ASSERT(j->advanceOutputs,
                "calloc(%" PRIu32 ",%zu) failed",
                j->numOutputs, sizeof(*j->advanceOutputs));

        // Initialize the args:
        //
        for(uint32_t i = j->numOutputs - 1; i != -1; --i) {
            struct QsBuffer *buffer = j->outputs[i].buffer;
            DASSERT(buffer);
            DASSERT(buffer->end);
            DASSERT(buffer->mapLength);
            DASSERT(buffer->mapLength >= buffer->overhangLength);
            DASSERT(buffer->overhangLength);
            //j->outputs[i].isFlushing = false;
            //
            // We can initialize the flow() output args:
            //
            // Set the write pointer to the start of the memory
            // mapping.
            j->outputBuffers[i] = buffer->end - buffer->mapLength;
            //
            // j->outputLens[i] gets set just before flow() or flush().
            //
            // j->advanceOutputs[] is 0 already.  Nothing has been
            // written yet.
            //
            j->outputs[i].isFlushing = false;
        }
    }

    j->isFinished = false;
}


// Also allocates the flow() call parameter arguments for all the
// blocks that need it.
//
static
void CreateBlockStreamArgs(struct QsBlock *b) {

    if(b->type == QsBlockType_simple) {
        struct QsSimpleBlock *sb = (void *) b;
        if(sb->streamJob) {
            CreateStreamArgs(sb->streamJob);
            ++b->graph->streamBlockCount;
        }
    } else if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(b = p->firstChild; b; b = b->nextSibling)
            CreateBlockStreamArgs(b);
    }
}


static void
QueueSourceJobs(struct QsBlock *b) {

    if(b->type == QsBlockType_simple) {
        struct QsStreamJob *j =
            ((struct QsSimpleBlock *) b)->streamJob;
        if(j && (!j->numInputs || j->isSource) && j->numOutputs
                && j->outputs[0].inputs)
            qsJob_queueJob((void *) j);
    }

    if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(struct QsBlock *b = p->firstChild; b; b = b->nextSibling)
            QueueSourceJobs(b);
    }
}


// The stream job can be 0.   A block can have a construct() and start()
// callback function without having any inputs or outputs.
//
static inline bool
CallBlockStart(struct QsSimpleBlock *b, struct QsStreamJob *j) {

    void *dlhandle = GetRunFileDlhandle(b);

    if(!dlhandle && b->runFile) {
        ERROR("Simple block \"%s\" failed to start",
                b->jobsBlock.block.name);
        return true; // true => fail
    }

    DASSERT(!b->started);
    b->started = true;

    if(!b->startChecked) {
        int ret = 0;
        // At least for this block the stream has not started yet ever
        // in this process.
        int (*construct)(void *userData) =
            GetBlockSymbol(dlhandle, b->module.fileName, "construct");
        // Call construct() if there is one.
        if(construct) {
            // This code is reentrant.  In this case it does not need to
            // be.
            struct QsWhichBlock stackSave;
            SetBlockCallback((void *) b, CB_CONSTRUCT, &stackSave);
            ret = construct(b->jobsBlock.block.userData);
            RestoreBlockCallback(&stackSave);
            if(ret <= -1) {
                ERROR("Block \"%s\" construct() failed (ret=%d)",
                        b->jobsBlock.block.name, ret);
                b->donotFinish = true;
            } else if(ret >= 1)
                INFO("Block \"%s\" construct() returned %d",
                        b->jobsBlock.block.name, ret);
        }
        b->start = GetBlockSymbol(dlhandle, b->module.fileName, "start");
        b->stop = GetBlockSymbol(dlhandle, b->module.fileName, "stop");
        b->startChecked = true;

        if(j) {
            j->flow = GetBlockSymbol(dlhandle, b->module.fileName, "flow");
            // We could not do this check before now, because a runFile
            // is not loaded until flow time.  A block that has stream
            // input and/or output without a flow() function is totally
            // broken.  So ya, fuck it.
            ASSERT(j->flow, "Stream connected block \"%s\" callback "
                    "flow() not found",
                    b->jobsBlock.block.name);
            // Get optional flush() if we can; if not that's fine.
            j->flush = GetBlockSymbol(dlhandle, b->module.fileName, "flush");
        }

        if(ret)
            return true; // true => fail
    }


    if(b->start && !b->donotFinish) {
        int ret;
        // This code is reentrant.  In this case it does not need to be.
        struct QsWhichBlock stackSave;
        SetBlockCallback((void *) b, CB_START, &stackSave);
        uint32_t numInputs = 0;
        uint32_t numOutputs = 0;
        if(b->streamJob) {
            numInputs = b->streamJob->numInputs;
            numOutputs = b->streamJob->numOutputs;
        }
        ret = b->start(numInputs, numOutputs,
                b->jobsBlock.block.userData);
        RestoreBlockCallback(&stackSave);
        if(ret <= -1) {
            ERROR("Block \"%s\" start() failed (ret=%d)",
                    b->jobsBlock.block.name, ret);
            b->donotFinish = true;
        }
        else if(ret >= 1)
            INFO("Block \"%s\" start() returned %d",
                    b->jobsBlock.block.name, ret);
        if(ret)
            return true; // true => fail
    }

    return false; // false => success
}



static
bool Start(struct QsBlock *b) {

    bool ret = false;

    if(b->type == QsBlockType_simple)
        if(CallBlockStart((void *) b,
                    ((struct QsSimpleBlock *) b)->streamJob))
            ret = true;

    if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(struct QsBlock *b = p->firstChild; b; b = b->nextSibling)
            if(Start(b))
                ret = true;
    }

    return ret; // false => success
}


static void
UnsetNumInputsAndNumOutputs(struct QsBlock *b) {

    // This is making the block's stream jobs into uninitialized stream
    // thingys.  There should be no stream ring buffers allocated now.
    if(b->type == QsBlockType_simple) {
        struct QsSimpleBlock *sb = (void *) b;
        if(sb->streamJob) {
            DASSERT(sb->streamJob->outputBuffers == 0);
            DASSERT(sb->streamJob->outputs || sb->streamJob->inputs);
            sb->streamJob->numInputs = 0;
            sb->streamJob->numOutputs = 0;
        }
    }

    if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        for(struct QsBlock *b = p->firstChild; b; b = b->nextSibling)
            UnsetNumInputsAndNumOutputs(b);
    }
}



int qsGraph_start(struct QsGraph *g) {

    NotWorkerThread();

    DASSERT(g);

    int ret = 0;

    CHECK(pthread_mutex_lock(&g->mutex));

    if(!g->parentBlock.firstChild ||
            !CheckForConnectedStreams((struct QsBlock *)g)) {
        // There's nothing to start.
        ret = 1;
        goto finish2;
    }

    if(g->runningStreams) {
        ret = 2;
        goto finish2;
    }

    ///////////////////////////////////////////////////////////
    // We make 7 loops through all the blocks in a very
    // particular order.
    ///////////////////////////////////////////////////////////

    // Halt just the stream job blocks and their peers.
    //
    uint32_t numHalts = HaltStreamBlocks((void *) g); // loop 1


    // We needed to halt before calling CheckStreamConnections() because
    // it changes variables in the graph.

    g->numInputs = 0;

    // CheckStreamConnections() also sets the simple blocks
    // streamJob::numInputs and streamJob::numOutputs just because we
    // could do that in this looping through the blocks.  We did not want
    // to add yet another looping through the blocks just for getting the
    // simple blocks streamJob::numInputs and streamJob::numOutputs.
    //
    if(CheckStreamConnections(g, &g->numInputs)) { // loop 2
        g->numInputs = 0;
        ret = 3;
        goto finish1;
    }

#if 0 // We do not have a good reason to not allow this.
      // We just give the user what they ask for, nothing but block
      // start() calls.
    if(!g->numInputs) {
        ret = 4;
        WARN("Graph \"%s\" has no stream ports connected", g->name);
        goto finish1;
    }
#endif

    // Stop the graph waiter thread while we do ...
    CHECK(pthread_mutex_lock(&g->cqMutex));

    g->streamBlockCount = 0; // We'll add them up.

    g->runningStreams = true;

    // The block's start() functions must be called before the stream
    // ring buffers are allocated, so that the block can know the
    // maximum port write size, in case it needed to make other buffers
    // to aid it's working.

    if(Start((void *) g)) { // loop 3
        // One of the block's construct() and/or start() calls returned
        // less than 0.
        ret = 5;
        WARN("A block stream start failed");
        CHECK(pthread_mutex_unlock(&g->cqMutex));
        // Reset the streamJob::numInputs and streamJob::numOutputs
        // otherwise we can't use qsGraph_stop(g) to call the needed
        // block stop() callbacks.
        UnsetNumInputsAndNumOutputs((void *) g);
        goto finish1;
    }


    CreateBlockPassThroughs((void *) g); // loop 4
    CreateBlockRingBuffers( (void *) g); // loop 5
    CreateBlockStreamArgs(  (void *) g); // loop 6

    CHECK(pthread_mutex_unlock(&g->cqMutex));

    QueueSourceJobs((void *) g); // loop 7

finish1:

    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(g);

finish2:

    if(g->feedback) {
        // The program ../bin/quickstreamGUI sets and uses this
        // g->feedback().
        if(ret)
            // It did not start
            g->feedback(g, QsStop, g->fbData);
        else
            g->feedback(g, QsStart, g->fbData);
    }

    if(ret && g->runningStreams) {
        // It failed and it is running, so we need to stop it.
        // Like it got half started and bailed.

        // Start() failed, so we need to qsGraph_stop() we'll keep the
        // recursive g->mutex lock while we do it.
        //
        // We need to call the block's stop() callbacks for those block's
        // which we called start() for; so we just use qsGraph_stop(g) to
        // do that.
        //
        // The g->feedback() stuff will be called and fixed in
        // qsGraph_stop(g).
        //
        qsGraph_stop(g);
        DASSERT(!g->runningStreams);
    }

    CHECK(pthread_mutex_unlock(&g->mutex));

    return ret;
}
