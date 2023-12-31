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



// Check if a block with stream job j would still be connected to the
// block b with the connection "input" disconnected.
//
// We do this "shit" because we do not have a reference count in the job
// peers; or something that keeps track of "connections" in the job API.
//
// We could add more memory to the struct QsJob to automatically do this,
// but this is a transit thing so this may be fine.
//
static
bool CheckBlockStreamConnected(const struct QsStreamJob *j,
        const struct QsInput *input, const struct QsJobsBlock *b) {

    DASSERT(input);
    DASSERT(j->job.jobsBlock);
    DASSERT(b);
    // We do not do this for the same block that owns j.
    DASSERT(b != (void *) j->job.jobsBlock);

    for(uint32_t i = j->maxInputs - 1; i != -1; --i) {
        struct QsInput *in = j->inputs + i;
        if(in == input) continue;
        if(in->output) {
            // This input, in, is connected.
            struct QsBlock *ob = in->output->port.block;
            DASSERT(ob);
            if(ob == &b->block)
                // It would be still connected.
                return true;
        }
    }

    for(uint32_t i = j->maxOutputs - 1; i != -1; --i) {
        struct QsOutput *out = j->outputs + i;
        for(uint32_t k = out->numInputs - 1; k != -1; --k) {
            struct QsInput *in = *(out->inputs + k);
            DASSERT(in->output == out);
            // We can have a block connect to itself.
            if(in == input) continue;
            struct QsBlock *ib = in->output->port.block;
            DASSERT(ib);
            if(ib == &b->block)
                // It would be still connected.
                return true;
        }
    }

    // If input was removed, it would not be connected.
    return false;
}


static inline int
DisconnectInput(struct QsStreamJob *j,
        struct QsInput *in, uint32_t portNum) {

    DASSERT(j);
    DASSERT(in);

    ASSERT(j->maxInputs > portNum);

    DASSERT(in >= j->inputs);
    DASSERT(in < j->inputs + j->maxInputs);
    DASSERT(in == j->inputs + portNum);

    struct QsOutput *out = in->output;
    DASSERT(out);

    // Remove this input, in, from its output.
    uint32_t i = 0;
    for(; i < out->numInputs; ++i)
        if(out->inputs[i] == in) 
            break;

    if(i == out->numInputs) {
        // If this was a valid port this port was not connected
        // to begin with, or we fucked up this code.
        ERROR("Block \"%s\" input %" PRIu32 " not connected",
                j->job.jobsBlock->block.name, portNum);
        return -1;
    }

    // We may be removing one peer job/block and not more than one; given
    // that we are removing just one input connection.
    //
    // See if we need to remove a job and its block from the job peers
    // in the stream connections to this input block.
    //
    if(in->port.block != out->port.block /*not the same block*/ &&
            !CheckBlockStreamConnected(j, in, (void *)out->port.block)) {

        struct QsStreamJob *rj =
            ((struct QsSimpleBlock *) out->port.block)->streamJob;
        DASSERT(rj);
        // Disconnect these two blocks; same as disconnect these two
        // stream jobs in the "job" API.
        qsJob_removePeer((void *) j, (void *) rj);
        // Mutually, remove the mutexes from each others' jobs.  They will
        // no longer queue each other, while they are not connected.
        qsJob_removeMutex((void *) j, &rj->mutex);
        qsJob_removeMutex((void *) rj, &j->mutex);
    }

    // Unmark the parent block that made the connection.
    in->port.parentBlock = 0;


    if(out->numInputs == 1) {
        // There no longer will be any connection from this output, out.
        //
        // Check and fix pass-through output linked list if it exists.
        if(out->next) {
            // to the next block.
            DASSERT(out->next->prev == out);
            out->next->prev = 0;
            out->next = 0;
        }
        if(out->prev) {
            // from the previous block.
            DASSERT(out->prev->next == out);
            out->prev->next = 0;
            out->prev = 0;
        }
        // That seems to cover all the (2) cases were braking a connection
        // disconnects a pass-through buffer line.
        //
        // Making a connection may add it back.
    }

    for(++i; i < out->numInputs; ++i)
        // Push the after array elements in by one.
        out->inputs[i-1] = out->inputs[i];

    --out->numInputs;
#ifdef DEBUG
    // Zero the element we are clipping.
    out->inputs[out->numInputs] = 0;
#endif


    if(out->numInputs) {
        out->inputs = realloc(out->inputs,
                out->numInputs*sizeof(*out->inputs));
        ASSERT(out->inputs, "realloc(,%zu) failed",
                out->numInputs*sizeof(*out->inputs));
    } else {
        // This output, out, has no more inputs connected.
        free(out->inputs);
        out->inputs = 0;
    }

    // Remove the output from the input.  Inputs only connect to one
    // output.
    in->output = 0;


    return 0; // 0 = success
}


static inline int
DisconnectOutput(struct QsStreamJob *j,
    struct QsOutput *out, uint32_t portNum) {

    DASSERT(j);
    DASSERT(out);

    // Do not shit on memory.
    ASSERT(j->maxOutputs > portNum);
    DASSERT(out >= j->outputs);
    DASSERT(out < j->outputs + j->maxOutputs);
    DASSERT(out == j->outputs + portNum);

    int ret = 0;

    // An output with no inputs has no connections.
    // An output can connect to many inputs.

    while(out->inputs) {
        DASSERT(out->numInputs);
        struct QsInput *in = *(out->inputs + out->numInputs - 1);
        struct QsStreamJob *j =
            ((struct QsSimpleBlock *) in->port.block)->streamJob;
        DASSERT(j);
        if(DisconnectInput(j, in, in->portNum)) {
            // This means these and or related code is wrong.
            // This would be a bug in this code.
            ASSERT(0, "Disconnecting non-existent connection");
            ret = -1;
        }
    }
    DASSERT(!out->numInputs);

    return ret; // 0 = success
}


void DestroyStreamJob(struct QsSimpleBlock *b, struct QsStreamJob *sj) {

    DASSERT(b->jobsBlock.block.graph->runningStreams == false);
    DASSERT(sj);


    if(sj->inputs)
        // Disconnect all inputs.
        for(uint32_t i = sj->maxInputs - 1; i != -1; --i)
            if(sj->inputs[i].output)
                ASSERT(0 == DisconnectInput(sj, sj->inputs + i, i));

    if(sj->outputs)
        // Disconnect all outputs.
        for(uint32_t i = sj->maxOutputs - 1; i != -1; --i) {
            struct QsOutput *out = sj->outputs + i;
            if(out->inputs) {
                DASSERT(out->numInputs);
                DisconnectOutput(sj, out, i);
            }
            DASSERT(out->prev == 0);
            DASSERT(out->next == 0);
        }


    qsJob_cleanup((void *) sj);

    CHECK(pthread_mutex_destroy(&sj->mutex));


    if(sj->inputs) {
        DASSERT(b->module.ports.inputs);
        for(uint32_t i = sj->maxInputs-1; i != -1; --i) {
            struct QsPort *port = &sj->inputs[i].port;
            DASSERT(port->name);
            DZMEM(port->name, strlen(port->name));
            free(port->name);
        }
        DZMEM(sj->inputs, sj->maxInputs*sizeof(*sj->inputs));
        free(sj->inputs);
    }
#ifdef DEBUG
    else {
        ASSERT(!sj->maxInputs);
    }
#endif

    if(sj->outputs) {
        DASSERT(b->module.ports.outputs);
        for(uint32_t i = sj->maxOutputs-1; i != -1; --i) {
            struct QsOutput *out = sj->outputs + i;
            struct QsPort *port = &out->port;
            DASSERT(port->name);
            DASSERT(!out->numInputs);
            DASSERT(!out->inputs);
            DZMEM(port->name, strlen(port->name));
            if(out->inputs)
                free(out->inputs);
            free(port->name);
        }

        DZMEM(sj->outputs, sj->maxOutputs*sizeof(*sj->outputs));
        free(sj->outputs);
    }
#ifdef DEBUG
    else {
        ASSERT(!sj->maxOutputs);
    }
#endif

    DZMEM(sj, sizeof(*sj));
    free(sj);
}


void qsSetNumInputs(uint32_t min, uint32_t max) {

    NotWorkerThread();

    if(min > max) {
        // Fix this miss-ordered arguments case.
        uint32_t x = max;
        max = min;
        min = x;
    }

    if(max == 0)
        // Dealing with a WTF case.
        return;

    struct QsSimpleBlock *b;
    struct QsStreamJob *sj = GetStreamJob(CB_DECLARE, &b);

    if(sj->maxInputs) {
        // We have been here before.
        ERROR("Block calling twice");
        return;
    }

    sj->maxInputs = max;
    sj->minInputs = min;

    sj->inputs = calloc(max, sizeof(*sj->inputs));
    ASSERT(sj->inputs, "calloc(%" PRIu32 ",%zu) failed",
            max, sizeof(*sj->inputs));

    for(uint32_t i = 0; i < max; ++i) {
        struct QsInput *input = sj->inputs + i;
        input->nextMaxRead = QS_DEFAULT_MAXREAD;
        input->maxRead = QS_DEFAULT_MAXREAD;
        input->portNum = i;
        struct QsPort *port = &input->port;
        port->portType = QsPortType_input;
        port->block = (void *) b;
        char name[12];
        snprintf(name, 12, "%" PRIu32, i);
        port->name = strdup(name);
        ASSERT(port->name, "strdup() failed");
        ASSERT(0 == qsDictionaryInsert(b->module.ports.inputs, name, port, 0));
    }
}


void qsSetNumOutputs(uint32_t min, uint32_t max) {

    NotWorkerThread();

    if(min > max) {
        // Fix this miss-ordered arguments case.
        uint32_t x = max;
        max = min;
        min = x;
    }

    if(max == 0)
        // Dealing with a WTF case.
        return;

    struct QsSimpleBlock *b;
    struct QsStreamJob *sj = GetStreamJob(CB_DECLARE, &b);

    if(sj->maxOutputs) {
        // We have been here before.
        ERROR("Block calling twice");
        return;
    }

    sj->maxOutputs = max;
    sj->minOutputs = min;

    sj->outputs = calloc(max, sizeof(*sj->outputs));
    ASSERT(sj->outputs, "calloc(%" PRIu32 ",%zu) failed",
            max, sizeof(*sj->outputs));

    for(uint32_t i = 0; i < max; ++i) {
        struct QsOutput *output = sj->outputs + i;
        output->maxWrite     = QS_DEFAULT_MAXWRITE;
        output->nextMaxWrite = QS_DEFAULT_MAXWRITE;
        output->portNum = i;
        struct QsPort *port = &output->port;
        port->portType = QsPortType_output;
        port->block = (void *) b;
        char name[12];
        snprintf(name, 12, "%" PRIu32, i);
        port->name = strdup(name);
        ASSERT(port->name, "strdup() failed");
        ASSERT(0 == qsDictionaryInsert(b->module.ports.outputs,
                    name, port, 0));
    }
}


void qsAdvanceInput(uint32_t inputPortNum, size_t len) {

    struct QsStreamJob *sj = GetStreamJob(CB_FLOW|CB_FLUSH, 0);

    ASSERT(inputPortNum < sj->numInputs);
    ASSERT(len);
    ASSERT(sj->advanceInputs[inputPortNum] == 0,
            "stream port cannot be advanced more"
            " than once per flow() call");
    ASSERT(len <= sj->inputLens[inputPortNum]);

    sj->advanceInputs[inputPortNum] = len;
}


void qsAdvanceOutput(uint32_t outputPortNum, size_t len) {

    struct QsStreamJob *sj = GetStreamJob(CB_FLOW|CB_FLUSH, 0);

    ASSERT(outputPortNum < sj->numOutputs);
    ASSERT(len);
    ASSERT(sj->advanceOutputs[outputPortNum] == 0,
            "stream port cannot be advanced more"
            " than once per flow() call");
    ASSERT(len <= sj->outputLens[outputPortNum]);

    sj->advanceOutputs[outputPortNum] = len;
}


void qsSetInputMax(uint32_t inputPortNum, size_t len) {

    NotWorkerThread();

    DASSERT(len);

    struct QsStreamJob *sj = GetStreamJob(CB_ANY, 0);

    // This must be.
    ASSERT(inputPortNum < sj->maxInputs);

    DASSERT(sj->inputs);
    DASSERT(sj->maxInputs);

    if(len == 0)
        len = 1;

    sj->inputs[inputPortNum].nextMaxRead = len;
}


void qsSetOutputMax(uint32_t outputPortNum, size_t maxWriteLen) {

    NotWorkerThread();

    struct QsStreamJob *sj = GetStreamJob(CB_ANY, 0);

    // This must be.
    ASSERT(outputPortNum < sj->maxOutputs);

    DASSERT(sj->outputs);
    DASSERT(sj->maxOutputs);

    if(maxWriteLen == 0)
        // Engineer out this bad case.  I cannot think of how 0 is
        // useful.
        maxWriteLen = 1;

    sj->outputs[outputPortNum].nextMaxWrite = maxWriteLen;
}


// The parameters to this function are a little redundant, but that's
// okay: this is not a public API interface and we need to get these
// variables anyway; to make sure things are consistent.
//
int qsBlock_connectStreams(struct QsSimpleBlock *inb, uint32_t inPort,
        struct QsSimpleBlock *outb, uint32_t outPort) {

    // TODO: add logging to the super block (or graph).

    NotWorkerThread();

    DASSERT(inb);
    DASSERT(outb);
    DASSERT(inb->jobsBlock.block.type == QsBlockType_simple);
    DASSERT(outb->jobsBlock.block.type == QsBlockType_simple);
    struct QsGraph *g = inb->jobsBlock.block.graph;
    DASSERT(g);
    DASSERT(outb->jobsBlock.block.graph == g);

    int ret = 0;


    // Recursive mutexes are great.
    CHECK(pthread_mutex_lock(&g->mutex));


    ASSERT(inb->streamJob);
    ASSERT(outb->streamJob);
    ASSERT(inb->streamJob->inputs);
    ASSERT(outb->streamJob->outputs);


    // These ports must exist.
    if(outPort >= outb->streamJob->maxOutputs) {
        ERROR("block \"%s\" bad output port %" PRIu32 " to high",
                outb->jobsBlock.block.name, outPort);
        ret = -1;
        goto finish;
    }

    if(inPort >= inb->streamJob->maxInputs) {
        ERROR("Block \"%s\" bad input port %" PRIu32 " to high",
                inb->jobsBlock.block.name, inPort);
        ret = -1;
        goto finish;
    }

    struct QsInput *in = inb->streamJob->inputs + inPort;
    struct QsOutput *out = outb->streamJob->outputs + outPort;


    if(in->output) {
        ERROR("Block \"%s\" input port %" PRIu32 " already connected",
                inb->jobsBlock.block.name, inPort);
        ret = 1;
        goto finish;
    }

    // This thread pool job abstraction takes care of some things
    // magically.  We cannot make stream connections while the stream is
    // running, but it's not because of the thread pool job abstraction.
    //
    // You can kind-of think of the thread pool halt lock as a write-lock
    // to editing the graph.
    //
    uint32_t numHalts = qsBlock_threadPoolHaltLock(&inb->jobsBlock);
    numHalts += qsBlock_threadPoolHaltLock(&outb->jobsBlock);

    // Mark the block which is doing this graph edit.  It could be a super
    // block or the graph (top block).  For the graph GetBlock() returns
    // 0.
    struct QsParentBlock *p = GetBlock(CB_DECLARE|CB_CONFIG, 0, 0);
    // Mark the parentBlock on the input, in.
    in->port.parentBlock = p?p:(void *) g;
    // We do not need to mark the output.


    if(g->runningStreams)
        // The big question here is: Do we start it again before returning
        // from this function?  I'd say no, where can be lots more
        // connection editing to follow this.
        //
        // Doing this after the halts will keep things "more atomic" for
        // multi-threaded control/runner programs, not that that will
        // exist; but who knows, I can dream.
        qsGraph_stop(g);


    // Add to the list of output to input connections in the output:
    ++out->numInputs;
    out->inputs = realloc(out->inputs,
            out->numInputs*sizeof(*out->inputs));
    ASSERT(out->inputs, "realloc(,%zu) failed",
            out->numInputs*sizeof(*out->inputs));

    // Connect them:
    out->inputs[out->numInputs-1] = in;
    in->output = out;


    if(inb != outb) {
        DASSERT(inb->streamJob != outb->streamJob);
        // Now deal with connecting the stream jobs and their mutexes.
        // The jobs API does not keep reference counts and lets you
        // request to add peers and mutexes more than once, while really
        // adding them just once.  NOTE: that makes disconnecting stream
        // connections more complicated; but maybe not too complicated.
        // There's always give and take; pros and cons.  We choose to keep
        // the jobs API simpler at the expense of making the streams code
        // just a little more complex and slower in the disconnecting,
        // transit, case.
        qsJob_addPeer((void *) inb->streamJob, (void *) outb->streamJob);
        qsJob_addMutex((void *) inb->streamJob, &outb->streamJob->mutex);
        qsJob_addMutex((void *) outb->streamJob, &inb->streamJob->mutex);
    }
#ifdef DEBUG
    else
        ASSERT(inb->streamJob == outb->streamJob);
#endif


    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(g);

finish:

    CHECK(pthread_mutex_unlock(&g->mutex));

    return ret;
}


int qsBlock_disconnectStreams(struct QsSimpleBlock *b,
        struct QsPort *port, uint32_t portNum) {

    // TODO: add logging to the super block (or graph)???

    NotWorkerThread();

    DASSERT(b);
    DASSERT(b->jobsBlock.block.type == QsBlockType_simple);
    struct QsGraph *g = b->jobsBlock.block.graph;
    DASSERT(g);

    int ret = 0;

    CHECK(pthread_mutex_lock(&g->mutex));

    // Hard check for a WTF.
    ASSERT(b->streamJob);

    // This is a little redundant; but debug so screw it, do it.
    DASSERT(port->portType == QsPortType_input ||
            port->portType == QsPortType_output);


    // Halt a subset of the graph.
    uint32_t numHalts = qsBlock_threadPoolHaltLock(&b->jobsBlock);


    if(g->runningStreams)
        // The big question here is: Do we start it again before returning
        // from this function?  I'd say no, where can be lots more
        // connection editing to follow this.
        //
        // Doing this after the halts will keep things "more atomic" for
        // multi-threaded control/runner programs, not that that will
        // exist; but who knows, I can dream.
        qsGraph_stop(g);


    switch(port->portType) {
        case QsPortType_input:
            ret = DisconnectInput(b->streamJob,
                    (void *) port, portNum);
            break;
        case QsPortType_output:
            ret = DisconnectOutput(b->streamJob,
                    (void *) port, portNum);
            break;
        default:
            ASSERT(0, "WTF");
            break;
    }

    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(g);


    CHECK(pthread_mutex_unlock(&g->mutex));

    return ret;
}


void qsUnmakePassThroughBuffer(uint32_t inPort) {

    NotWorkerThread();

    struct QsSimpleBlock *b;
    struct QsStreamJob *sj = GetStreamJob(CB_DECLARE|CB_CONFIG, &b);

    // These ports must exist.  They do not need to be connected, but they
    // must exist.
    ASSERT(sj->maxOutputs);
    ASSERT(inPort < sj->maxInputs);
    DASSERT(sj->outputs);
    DASSERT(sj->inputs);

    if(!sj->inputs[inPort].passThrough)
        // Unmake not needed.  It was never set.
        return;

    if(b->jobsBlock.block.graph->runningStreams)
        INFO("Stream is running. Unmaking a pass-through buffer: "
                "Block \"%s\" input %" PRIu32 " to output %s"
                " for the next stream run",
                b->jobsBlock.block.name,
                inPort,
                sj->inputs[inPort].passThrough->port.name);
    else
        DSPEW("Unaking a pass-through buffer: "
                "Block \"%s\" input %" PRIu32 " to output %s",
                b->jobsBlock.block.name, inPort,
                sj->inputs[inPort].passThrough->port.name);

    DASSERT(sj->inputs[inPort].passThrough->passThrough ==
            sj->inputs + inPort);
    // Unset the output to input marker.
    sj->inputs[inPort].passThrough->passThrough = 0;
    // Unset the input to output marker.
    sj->inputs[inPort].passThrough = 0;
}


void qsMakePassThroughBuffer(uint32_t inPort, uint32_t outPort) {

    NotWorkerThread();

    struct QsSimpleBlock *b;
    struct QsStreamJob *sj = GetStreamJob(CB_DECLARE|CB_CONFIG, &b);

    // These ports must exist. They do not need to be connected, but they
    // must exist.
    ASSERT(outPort < sj->maxOutputs);
    ASSERT(inPort < sj->maxInputs);
    DASSERT(sj->outputs);
    DASSERT(sj->inputs);

    if(sj->inputs[inPort].passThrough == sj->outputs + outPort &&
            sj->outputs[outPort].passThrough == sj->inputs + inPort)
        // We already have this setting.
        return;

    ASSERT(!sj->inputs[inPort].passThrough,
            "You cannot have a strream input port (%" PRIu32
            ") block \"%s\" with more than one pass-through buffer",
            inPort, b->jobsBlock.block.name);
    ASSERT(!sj->outputs[outPort].passThrough,
            "You cannot have a strream output port (%" PRIu32
            ") block \"%s\" with more than one pass-through buffer",
            outPort, b->jobsBlock.block.name);

    if(b->jobsBlock.block.graph->runningStreams)
        INFO("Stream is running. Making a pass-through buffer: "
                "Block \"%s\" input %" PRIu32 " to output %" PRIu32
                " for the next stream run",
                b->jobsBlock.block.name, inPort, outPort);
    else
        DSPEW("Making a pass-through buffer: "
                "Block \"%s\" input %" PRIu32 " to output %" PRIu32,
                b->jobsBlock.block.name, inPort, outPort);

    sj->inputs[inPort].passThrough = &sj->outputs[outPort];
    sj->outputs[outPort].passThrough = &sj->inputs[inPort];
}


void qsOutputDone(uint32_t outputPortNum) {

    struct QsStreamJob *sj = GetStreamJob(CB_FLOW|CB_FLUSH, 0);

    ASSERT(outputPortNum < sj->numOutputs);

    qsJob_lock((void *) sj);

    ASSERT(!sj->outputs[outputPortNum].isFlushing);

    sj->outputs[outputPortNum].isFlushing = true;
    // Mark the stream IO as changed.
    sj->didIOAdvance = true;

    qsJob_unlock((void *) sj);
}
