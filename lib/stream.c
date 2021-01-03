// For functions related to connecting blocks via a stream connection.
// These streams we refer to are a way for blocks to exchange data that is
// like a continuous stream, like a very long array that functions just get
// pointers to.  It does not require a constant rate for quickstream, we
// let the hardware do whatever it requires.  If the hardware is required
// to "stream" data at a constant we let it.
//
// Parameter connection are another kind of connection.  Parameter data is
// exchanged in small discrete chunks like parameters copied into the
// parameter arguments of a function.
//

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#include "../include/quickstream/app.h"
#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"

#include "debug.h"
#include "Dictionary.h"
#include "parameter.h"
#include "block.h"
#include "graph.h"
#include "trigger.h"
#include "threadPool.h"
#include "triggerJobsLists.h"



int qsBlockConnect(struct QsBlock *_from, struct QsBlock *_to,
        uint32_t fromPortNum, uint32_t toPortNum) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    DASSERT(_from);
    DASSERT(_to);
    DASSERT(!_from->isSuperBlock);
    DASSERT(!_to->isSuperBlock);

    if(_from->isSuperBlock || _to->isSuperBlock) {
        ASSERT(0, "Write code to connect SuperBlocks by a stream");
        return -1; // error
    }

    if(_to == _from) {
        ERROR("A block cannot connect to itself");
        return -2; // error
    }

    struct QsSimpleBlock *to = (struct QsSimpleBlock *) _to;
    struct QsSimpleBlock *from = (struct QsSimpleBlock *) _from;

    DASSERT(from->flow, "Block \"%s\" does not have a flow() function",
                    _from->name);
    DASSERT(to->flow, "Block \"%s\" does not have a flow() function",
                    _to->name);

    if(!to->flow || !from->flow) {
        if(!from->flow)
            ERROR("Block \"%s\" does not have a flow() function",
                    _from->name);
        if(!to->flow)
            ERROR("Block \"%s\" does not have a flow() function",
                    _to->name);
        return -3;
    }

    struct QsGraph *g = _from->graph;
    DASSERT(g);
    DASSERT(_to->graph == g);
    if(_to->graph != g || !g) {
        ERROR("Blocks are not from the same graph");
        return -4; // error
    }

    // Check that the input port is not occupied already.
    if(to->numInputs > toPortNum && to->inputs[toPortNum].feederBlock) {
        // There is a block that is feeding this port already.
        // An input port cannot have more than one connection to it.
        ERROR("Block %s input port %" PRIu32
                " is feed already by block %s from its' %" PRIu32
                " output port", _to->name, toPortNum,
                to->inputs[toPortNum].feederBlock->block.name,
                to->inputs[toPortNum].outputPortNum);
        return -5; // error
    }
    // It's okay if an output port has a connection already.

    // Make sure that from->outputs has the needed number of ports.
    // Port numbers start at 0, just like the array index that it is.
    if(from->numOutputs <= fromPortNum) {
        // We need to add an output port.  We'll check that all output
        // ports in the array get used before we start the streams.
        uint32_t oldNum = from->numOutputs;
        from->numOutputs = fromPortNum + 1;
        from->outputs = realloc(from->outputs,
                from->numOutputs*sizeof(*from->outputs));
        ASSERT(from->outputs, "realloc(,%zu) failed",
                from->numOutputs*sizeof(*from->outputs));
        // Realloc() does not initialize, so we do here, just the ones
        // that we just added.  Any old memory is saved by realloc().
        memset(from->outputs + oldNum, 0,
                (from->numOutputs - oldNum)*sizeof(*to->outputs));
        // Initialize all the outputs that we just added with the
        // output defaults that are not zero.
        for(uint32_t i = oldNum; i < from->numOutputs; ++i)
            from->outputs[i].maxWrite = QS_DEFAULT_OUTPUT_MAXWRITE;
    }

    // Make sure that to->inputs has the needed number of ports.
    // Port numbers start at 0, just like the array index that it is.
    if(to->numInputs <= toPortNum) {
        // We need to add an input port.  We'll check that all input
        // ports in the array get used before we start the streams.
        uint32_t oldNum = to->numInputs;
        to->numInputs = toPortNum + 1;
        // This will likely change the addresses of the inputs that the
        // outputs need to know about.
        to->inputs = realloc(to->inputs,
                to->numInputs*sizeof(*to->inputs));
        ASSERT(to->inputs, "realloc(,%zu) failed",
                to->numInputs*sizeof(*to->inputs));
        // Realloc() does not initialize, so we do here, just the ones
        // that we just added.  Any old memory is saved by realloc().
        memset(to->inputs + oldNum, 0,
                (to->numInputs - oldNum)*sizeof(*to->inputs));
        // Initialize all the inputs that we just added with the
        // input defaults that are not zero.
        for(uint32_t i = oldNum; i < to->numInputs; ++i) {
            struct QsInput *input = to->inputs + i;
            input->threshold = QS_DEFAULT_INPUT_THRESHOLD;
            input->maxRead = QS_DEFAULT_INPUT_MAXREAD;
        }
    }

    // Now connect the blocks.

    struct QsOutput *output = from->outputs + fromPortNum;
    struct QsInput *input = to->inputs + toPortNum;

    // Add a count of the number of inputs that an output will feed stream
    // data.
    //
    // Each output port keeps a list of the to block inputs which we will
    // allocate just before flow start.  We cannot allocate them now since
    // the addresses of the inputs can be changed each time a connection
    // is added (realloc()) above.  We just count them for now.
    ++output->numInputs;
    // We make this array of pointers from the output to the inputs just
    // before we run the flow, and remove it just after.
    DASSERT(output->inputs == 0);

    input->block = to;
    input->feederBlock = from;
    input->inputPortNum = toPortNum;
    input->outputPortNum = fromPortNum;

    return 0; // success
}


// We only need the block with the input port to remove a connection,
// because each input port can only have one connection.
//
int qsBlockDisconnect(struct QsBlock *b, uint32_t inputPortNum) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    DASSERT(b);
    DASSERT(b->graph);
    DASSERT(!b->isSuperBlock);

    ASSERT(b->graph->flowState == QsGraphPaused);

    if(b->isSuperBlock) {
        ASSERT(0, "Write code to connect SuperBlocks by a stream");
        return -1; // error
    }

    // Verify there is a connection.
    struct QsSimpleBlock *to = (struct QsSimpleBlock *) b;


    if(to->numInputs <= inputPortNum) {
        ERROR("There is no connection to block %s into port %" PRIu32,
                b->name, inputPortNum);
        return -2;
    }

    struct QsSimpleBlock *from = to->inputs[inputPortNum].feederBlock;
    DASSERT(from);
    uint32_t outputPortNum = to->inputs[inputPortNum].outputPortNum;
    DASSERT(from->numOutputs > outputPortNum);

    struct QsOutput *output = from->outputs + outputPortNum;

    // output->inputs array of pointers gets allocated just before running
    // the stream and destroyed just after.
    DASSERT(output->inputs == 0);
    // There should be no ring buffer at this time either.
    DASSERT(output->buffer == 0);
    // This input must be counted in the output thingy.
    DASSERT(output->numInputs);

    --output->numInputs;

    if(output->numInputs == 0) {

        // We cannot shuffle outputs array[] so it has no empty elements
        // in it, because the user specified the port numbers.  It's up to
        // the user to not leave empty slots in this array.  If this array
        // element is not on the end then we just zero it out.  Ya, in the
        // case where we end up free this memory this is wasted effort,
        // but this is simpler.
        memset(output, 0, sizeof(*output));

        // Only if the last element in the array is the one being nulled,
        // do we need to remove it.
        if(from->numOutputs == outputPortNum + 1) {
            // This is the end element in the outputs[] array.
            // Figure out how many elements are zeroed after this one.
            uint32_t newNumOutputs = from->numOutputs - 1;
            for(;newNumOutputs; --newNumOutputs)
                if(from->outputs[newNumOutputs - 1].numInputs)
                    break;
            from->numOutputs = newNumOutputs;

            if(newNumOutputs) {
                from->outputs = realloc(from->outputs,
                        newNumOutputs*sizeof(*from->outputs));
                ASSERT(from->outputs, "realloc(,%zu) failed",
                        newNumOutputs*sizeof(*from->outputs));
            } else {
                // We will have no more outputs
                free(from->outputs);
                from->outputs = 0;
            }
        }
    }

    // 2. Remove "to" input data from port inputPortNum
    //
    struct QsInput *input = to->inputs + inputPortNum;

    // inputs can only have one input connection, so we will null
    // that connection, no ifs about it.
    memset(input, 0, sizeof(*input));

    // Only if the last element in the array is the one being nulled, do
    // we need to remove it.
    if(to->numInputs == inputPortNum + 1) {
        // This is the end element in the inputs[] array.
        // Figure out how many elements are zeroed after this one.
        uint32_t newNumInputs = to->numInputs - 1;
        for(;newNumInputs; --newNumInputs)
            if(to->inputs[newNumInputs - 1].feederBlock)
                break;

        to->numInputs = newNumInputs;

        if(newNumInputs) {
            to->inputs = realloc(to->inputs,
                    newNumInputs*sizeof(*to->inputs));
            ASSERT(to->inputs, "realloc(,%zu) failed",
                    newNumInputs*sizeof(*to->inputs));
        } else {
            // We have no more inputs.
            free(to->inputs);
            to->inputs = 0;
        }
    }

    return 0; // success
}


// Returns true if there is a loop.
static bool
HaveLoops(struct QsSimpleBlock *b, uint32_t numFilters,
        uint32_t count) {

    for(int32_t i = b->numOutputs - 1; i != -1; --i) {
        ++count;
        if(count > numFilters)
            // We have been to count blocks greater than the number of
            // filters, therefore we are looping.  The recursive function
            // would blow the stack if not for this return.
            return true;
        for(int32_t j = b->outputs[i].numInputs - 1; j != -1; --j)
            return HaveLoops(b->outputs[i].inputs[j]->block,
                    numFilters, count);
    }
    return false;
}


static inline
int IsSource(const struct QsSimpleBlock *b) {
    // Sources have no input and some output:
    return (b->numInputs == 0 && b->numOutputs);
}


// We have a threadPool mutex lock when this is called.
static inline bool
CheckBlockFlowCallable(struct QsSimpleBlock *b) {

    DASSERT(b->streamTrigger);

    if(b->callingFlow || !b->streamTrigger->isRunning)
        // This filter block has a worker thread actively calling flow(),
        // so there is no need to queue a worker for it.  It will just
        // keep on working until working or queuing is not necessary, so
        // we don't and can't queue it now.
        return false;


    // If any outputs are clogged than we cannot call flow().
    for(uint32_t i = b->numOutputs-1; i != -1; --i) {
        struct QsOutput *output = b->outputs + i;
        for(uint32_t j = output->numInputs-1; j != -1; --j)
            if(output->inputs[j]->readLength >= output->maxLength)
                // We have at least one clogged output reader.  It has a
                // full amount that it can read.  And so we will not be
                // able to call flow().  Otherwise we could overrun the
                // read pointer with the write pointer.
                return false;
    }


    // If any input meets the threshold we can call flow() for this filter
    // block, b, we return true.  If this filter block, b, decides that
    // this one simple threshold condition is not enough then that filter
    // block's flow() call can just do nothing and than this will try
    // again later when another feeding filter block returns from a flow()
    // call and we do this again.
    for(uint32_t i = b->numInputs-1; i != -1; --i) {
        struct QsInput *input = b->inputs + i;
        if(input->readLength >= input->threshold)
            return true;
    }

    if(b->numInputs == 0)
        // We have no inputs so the inputs are not a restriction.
        return true;

    return false;
}


// This does a lot.
//
// Returns true to continue calling flow() or flush().
//
static inline
bool IsFlowableAndSetPointers(struct QsSimpleBlock *b) {

    // Both the inputs and the outputs must be "OK" in order to call
    // flow() after this call.
    //
    // If after all this looping/checking inputs and outputs we have all 3
    // of these bools true (in addition to the non-zero flow() return),
    // we can then continue to call flow() for this filter block.
    //
    // 1. inputAdvanced will be changed to true if:
    //
    //    a) one of the b->advanceLens[] has a non-zero value, otherwise
    //       there was no input consumed by the last flow() call, or 
    //
    //    b) there was added input data in at least one port since the
    //       last flow() call for this block, or
    //
    //    c) this filter block is a source that has no input ports.
    //
    bool inputAdvanced = (!b->numInputs)?true:false;
    //
    // 2. inputsFeeding will be changed to true if:
    //
    //    a) one input threshold is met, or
    //
    //    b) this is a source that has no input ports.
    //
    bool inputsFeeding = (!b->numInputs)?true:false;
    //
    // 3. allOutputsHungry will be changed to false if one output is full;
    //    We cannot continue to run flow() if one output is full, because
    //    that may cause a circular buffer overrun.
    //
    bool allOutputsHungry = true;

    // In addition to setting these flags we need to move the stream
    // pointers and stuff, to make the stream graph run.
    

    // Advance the output write pointers and see if we can write more.
    //
    // To be able to write more we must be able to write maxLength to all
    // output readers; because that's what this API promises the filter
    // block it can do.
    //
    // And grow the reader filter block's readLength.
    for(uint32_t i=b->numOutputs-1; i!=-1; --i) {

        struct QsOutput *output = b->outputs + i;

        // This should have been checked already in qsGetOutputBuffer()
        // and in qsOutput() hence the DASSERT() and not ASSERT().  Check
        // that the filter block, b, did not write more than promised.
        DASSERT(b->outputLens[i] <= output->maxWrite,
                "Filter block \"%s\" wrote %zu which is greater"
                " than the %zu promised",
                b->block.name, b->outputLens[i], output->maxWrite);

        // Advance write pointer.
        output->writePtr += b->outputLens[i];
        if(output->writePtr >= output->buffer->end) {
            output->writePtr -= output->buffer->mapLength;
            DASSERT(output->writePtr < output->buffer->end);
        }

        for(uint32_t j = output->numInputs-1; j != -1; --j) {
            struct QsInput *input = output->inputs[j];
            // Grow the reader filter block's readLength.  We may not
            // write to the current working job, but the readLength will
            // be either added to the working job, or it will become a
            // stream queued job later.
            //
            // Tally the read length in the filter block (not b) we are
            // feeding.  input->readLength is the length that the input
            // can read in a flow() call.
            input->readLength += b->outputLens[i];

            if(input->readLength >= output->maxLength && allOutputsHungry)
                // We have at least one clogged output reader.  It has
                // a full amount that it can read.  And so we will not
                // be continuing to call flow().  Otherwise we could
                // overrun the read pointer with the write pointer.
                allOutputsHungry = false;
        }
    }


    // Advance the read pointers that feed this block, b; and tally the
    // readers remaining length.
    for(uint32_t i = b->numInputs-1; i != -1; --i) {
        struct QsInput *input = b->inputs + i;

        if((b->advanceLens[i] || input->readLength > b->inputLens[i]
                    ) && !inputAdvanced)
            inputAdvanced = true;


        // The last time we had the stream mutex lock this was true, but
        // readLength may have changed while this thread was calling the
        // block's, b, flow() function:
        //
        // We had:
        // b->inputLens[i] = b->inputs[i]->readLength;
        //
        // b->inputs[i]->readLength may have increased.

        DASSERT(b->advanceLens[i] <= b->inputLens[i]);
        DASSERT(b->inputLens[i] <= input->readLength);

        if(b->inputLens[i] >= input->maxRead)
            // This block filter module is not written correctly.
            ASSERT(b->advanceLens[i],
                    "The filter block \"%s\" did not keep it's read promise"
                    " for input port %" PRIu32,
                    b->block.name, i);

        // Advance read pointer 
        input->readPtr += b->advanceLens[i];
        // Record the length that we have left to read up to the write
        // pointer (at this pass-through level).
        //
        // b is the reading filter.
        input->readLength -= b->advanceLens[i];

        if(input->readPtr >= input->buffer->end) {
            // Wrap the read pointer back in the circular buffer back
            // toward the start.
            input->readPtr -= input->buffer->mapLength;
            DASSERT(input->readPtr < input->buffer->end);
        }

        if(!inputsFeeding && input->readLength >= input->threshold)
            // The amount of input data left meets the needed threshold in
            // at least one input.  If the threshold condition is more
            // complex than the filter block will not eat the data we send
            // by just not advancing the buffer.
            inputsFeeding = true;
    }


    return (inputAdvanced && inputsFeeding && allOutputsHungry);
}


// This function is the source stream using the QsTrigger utility to make
// a "source trigger" abstraction which is underneath a block source which
// calls a blocking read(2) or something like that.  The writer of the
// the filter block was too much of a dumb ass to use the epoll_wait(2)
// wrapper thingy.  It's not as fucking stupid as GNU radio's one thread
// per block idea.
//
// Returns 1 to have source flow() stopped being called for this flow
// cycle.
//
// Returns 0 to keep running in this flow cycle.
//
// This needs to let other blocks get worker threads by not looping
// forever.  Even when there is only one worker thread, that will
// naturally happen when the output quick-stream gets clogged with too
// much data.  The ring buffer settings with determine when that is.
// Adjusting the ring buffer settings can optimise performance, and here
// is where that happens.
//
static
int StreamFlow_callback(struct QsSimpleBlock *b) {

    // We have threadPool mutex lock now ...

    DASSERT(b->flow);
    DASSERT(b->callingFlow == false);

    // Mark that no other worker thread should call this block's flow() or
    // queue up this block's stream flow() call.  There is no need to queue
    // it up, because it is actively running flow() until it can't.
    //
    // TODO: Can b->block.inWhichCallback = _QS_IN_FLOW replace this?
    //
    b->callingFlow = true;

    int ret;
    bool isFlowable;

    do {

        // Ready the arguments for calling flow():
        for(uint32_t i = b->numInputs-1; i != -1; --i) {
            b->inputLens[i] = b->inputs[i].readLength;
            b->advanceLens[i] = 0;
            b->inputBuffers[i] = b->inputs[i].readPtr;
        }
        for(uint32_t i = b->numOutputs-1; i != -1; --i)
            b->outputLens[i] = 0;

        DASSERT(b->block.inWhichCallback == _QS_IN_NONE);
        b->block.inWhichCallback = _QS_IN_FLOW;


        ///////////////////// UNLOCK ////////////////////////
        CHECK(pthread_mutex_unlock(b->threadPool->mutex));
        // and now we don't have the lock.

        ret = b->flow(b->inputBuffers, b->inputLens,
                b->numInputs, b->numOutputs);

        ///////////////////// LOCK //////////////////////////
        CHECK(pthread_mutex_lock(b->threadPool->mutex));
        // now we have the lock again.

        DASSERT(b->block.inWhichCallback == _QS_IN_FLOW);
        b->block.inWhichCallback = _QS_IN_NONE;

        isFlowable = IsFlowableAndSetPointers(b);

        // Add jobs to the stream job queue if we can, for filters we are
        // feeding.
        for(uint32_t i = b->numOutputs-1; i != -1; --i) {
            struct QsOutput *output = b->outputs + i;
            for(uint32_t j = output->numInputs-1; j != -1; --j) {
                struct QsSimpleBlock *smB = output->inputs[j]->block;
                if(CheckBlockFlowCallable(smB) &&
                        !smB->streamTrigger->isInJobQueue)
                    CheckAndQueueTrigger(smB->streamTrigger);
            }
        }

        // Add jobs to the stream job queue if we can, for filters that are
        // feeding this filter, b.
        for(uint32_t i = b->numInputs-1; i != -1; --i) {
            struct QsSimpleBlock *smB = b->inputs[i].feederBlock;
            if(CheckBlockFlowCallable(smB) &&
                        !smB->streamTrigger->isInJobQueue)
                CheckAndQueueTrigger(smB->streamTrigger);
        }


    } while(isFlowable && ret == 0);


    // Release the claim on the flow so another thread may queue this
    // block for another flow() or flush() call at a later time.
    b->callingFlow = false;


    if(ret) {
        if(ret > 0)
            DSPEW("Finished calling source block \"%s\" flow",
                    b->block.name);
        else
            ERROR("source block \"%s\" flow returned error",
                    b->block.name);
    }

    // Return:
    //   0   to continue regular running
    //   > 0 to stop triggering this block
    //   < 0 on error

    return ret;
}


static void
AllocateFlowOutputArgs(struct QsSimpleBlock *b) {

    if(b->numOutputs == 0) return;

    DASSERT(b->outputLens == 0);
    b->outputLens = calloc(b->numOutputs, sizeof(*b->outputLens));
    ASSERT(b->outputLens, "calloc(%" PRIu32 ",%zu) failed",
            b->numOutputs, sizeof(*b->outputLens));
}


static void
FreeFlowOutputArgs(struct QsSimpleBlock *b) {

    if(b->numOutputs == 0) return;

    DASSERT(b->outputLens);
#ifdef DEBUG
    memset(b->outputLens, 0, b->numOutputs*sizeof(*b->outputLens));
#endif
    free(b->outputLens);
    b->outputLens = 0;
}


static void
AllocateFlowInputArgs(struct QsSimpleBlock *b) {

    if(b->numInputs == 0) return;

    DASSERT(b->advanceLens == 0);
    b->advanceLens = calloc(b->numInputs, sizeof(*b->advanceLens));
    ASSERT(b->advanceLens, "calloc(%" PRIu32 ",%zu) failed",
            b->numInputs, sizeof(*b->advanceLens));

    DASSERT(b->inputBuffers == 0);
    b->inputBuffers = calloc(b->numInputs, sizeof(*b->inputBuffers));
    ASSERT(b->inputBuffers, "calloc(%" PRIu32 ",%zu) failed",
            b->numInputs, sizeof(*b->inputBuffers));

    DASSERT(b->inputLens == 0);
    b->inputLens = calloc(b->numInputs, sizeof(*b->inputLens));
    ASSERT(b->inputLens, "calloc(%" PRIu32 ",%zu) failed",
            b->numInputs, sizeof(*b->inputLens));
}


static void
FreeFlowInputArgs(struct QsSimpleBlock *b) {

    if(b->numInputs == 0) return;

    DASSERT(b->advanceLens);
    DASSERT(b->inputBuffers);
    DASSERT(b->inputLens);
#ifdef DEBUG
    memset(b->advanceLens, 0, b->numInputs*sizeof(*b->advanceLens));
    memset(b->inputBuffers, 0, b->numInputs*sizeof(*b->inputBuffers));
    memset(b->inputLens, 0, b->numInputs*sizeof(*b->inputLens));
#endif
    free(b->advanceLens);
    b->advanceLens = 0;
    free(b->inputBuffers);
    b->inputBuffers = 0;
    free(b->inputLens);
    b->inputLens = 0;
}


// Check the streams for loops and allocate output/input data structures;
// all but the ring buffers.  Ring buffers must be made after all the
// block start()s are called.
//
// This also allocates the arrays of function parameters that are passed
// to flow() and flush() calls, in AllocateFlowInputArgs() and
// AllocateFlowOutputArgs().
//
int StreamsInit(struct QsGraph *g) {

    DASSERT(g);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    // Find all sources
    uint32_t numSources = 0;
    DASSERT(g->sources == 0);

    g->numFilters = 0;

    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
        AllocateFlowInputArgs(smB);
        AllocateFlowOutputArgs(smB);
        if(IsSource(smB)) {
            ++numSources;
            if(!smB->userMadeTrigger) {
                DASSERT(smB->streamTrigger == 0);
                smB->streamTrigger = AllocateTrigger(
                        sizeof(struct QsStreamSource),
                        smB, QsStreamSource,
                        (int (*)(void *))StreamFlow_callback,
                        smB/*userData*/, 0/*checkTrigger*/,
                        true/*isSource*/);
                // Set this flag that is what it says, keep the threadPool
                // mutex lock when calling the callback().  The callback()
                // == StreamFlow_callback() knows about the lock and
                // unlocks and locks it when it needs to.
                smB->streamTrigger->keepLockInCallback = true;
            }
            DASSERT(smB->numOutputs);
        } else if(smB->numInputs || smB->numOutputs) {
            DASSERT(smB->streamTrigger == 0);
            smB->streamTrigger = AllocateTrigger(
                    sizeof(struct QsStreamIO),
                    smB, QsStreamIO,
                    (int (*)(void *))StreamFlow_callback,
                    smB/*userData*/, 0/*checkTrigger*/,
                    false/*isSource*/);
            smB->streamTrigger->keepLockInCallback = true;
        }
        if(smB->numInputs || smB->numOutputs)
            ++g->numFilters; // has input and/or output
    }

    if(g->numFilters == 0) return 0; // success


    if(numSources == 0) {
        ERROR("There are %" PRIu32 " filter blocks and"
                " none are sources", g->numFilters);
        return -1;
    }

    // The block's outputs need pointers to the inputs that they feed.
    if(MakeOuputInputsArray(g)) return -1;


    uint32_t i = 0;

    g->sources =
            malloc((numSources+1)*sizeof(*g->sources));
    ASSERT(g->sources, "malloc(%zu) failed",
            (numSources+1)*sizeof(*g->sources));

    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        if(IsSource((struct QsSimpleBlock *) b))
            g->sources[i++] = (struct QsSimpleBlock *)b;
    }
    g->sources[i] = 0; // zero terminate

    // dummy iterator, s
    struct  QsSimpleBlock **s = g->sources;

    // Check that the stream graph has no loops, if it does we fail.
    struct QsSimpleBlock *b = *s;
    for(; b; b = *(++s))
        if(HaveLoops(b, g->numFilters, 0))
            break;
    if(b) {
#ifdef DEBUG
        memset(g->sources, 0, numSources*sizeof(*g->sources));
#endif
        free(g->sources);
        g->sources = 0;
        g->numFilters = 0;
        RemoveOutputInputsArray(g);
        ERROR("There are loops starting from block \"%s\"",
                b->block.name);
        return -1;
    }


    // Set the maxWrite for all outputs from the "OutputMaxWrite"
    // parameter.  This can be overwritten in the block's start()
    // function.  It's a common parameter that the end user can set for
    // all simple blocks.  "OutputMaxWrite" ends up being a reserved
    // and universal simple block parameter.
    for(struct QsBlock *B = g->firstBlock; B; B = B->next) {
        if(B->isSuperBlock) continue;
        b = (struct QsSimpleBlock *) B;
        for(uint32_t i = b->numOutputs-1; i != -1; --i) {
            size_t maxWrite = 0;
            struct QsParameter *p = qsParameterGetPointer(B,
                    "OutputMaxWrite", false/*is setter?*/);
            DASSERT(p);
            DASSERT(p->size == sizeof(maxWrite));
            qsParameterGetValue(p, &maxWrite);
            if(maxWrite == 0)
                // 0 will not work.  TODO: but maybe it can be a special
                // value.
                maxWrite = QS_DEFAULTMAXWRITE;
            b->outputs[i].maxWrite = maxWrite;
        }
    }

    return 0;
}


// Allocate stream ring buffers and mmap() them, and queue the stream
// sources.
void StreamsStart(struct QsGraph *g) {


    if(g->numFilters == 0) {
        DASSERT(g->sources == 0);
        // There are no streams.
        return;
    }

    DASSERT(g->sources);

    // Allocate buffers and map ring buffers, now that we know the sizes.
    CreateRingBuffers(g);

    // Queue up jobs for all the QsStreamSource trigger source blocks.
    //
    // We did not do this when we created them because of the failure
    // modes above.
    //
    // dummy iterator, s
    struct  QsSimpleBlock **s = g->sources;
    //
    for(struct QsSimpleBlock *b = *s; b; b = *(++s)) {
        DASSERT(b->streamTrigger);
        if(b->streamTrigger->kind == QsStreamSource)
            //
            // Now queue the job for a worker.
            CheckAndQueueTrigger(b->streamTrigger);
    }
}


// Free the stream ring buffers and ...
void StreamStop(struct QsGraph *g) {

    DASSERT(g);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    if(g->numFilters == 0) {
        DASSERT(g->sources == 0);
        return;
    }

    DestroyRingBuffers(g);

    RemoveOutputInputsArray(g);

    // Free Args arrays and remove the stream triggers
    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
        FreeFlowInputArgs(smB);
        FreeFlowOutputArgs(smB);
        if(smB->streamTrigger) {
            FreeTrigger(smB->streamTrigger);
            smB->streamTrigger = 0;
        }
    }

    DASSERT(g->sources);
#ifdef DEBUG
    for(struct QsSimpleBlock **s = g->sources; *s; ++(s))
        *s = 0;
#endif
    free(g->sources);
    g->sources = 0;
    g->numFilters = 0;
}
