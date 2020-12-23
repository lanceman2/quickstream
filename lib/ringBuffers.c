// Makes the ring buffers.  The hard part is figuring out the size of
// them.
//
// It doesn't appear that GNUradio varies the length of the ring buffer.
// In quickstream the length of the ring buffers depends on the read and
// write promises, and whither or not the buffer is a pass-through buffer
// or not, as in ringBuffers.c.

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#include "../include/quickstream/app.h"
#include "../include/quickstream/builder.h"
#include "../include/quickstream/block.h"

#include "debug.h"
#include "Dictionary.h"
#include "block.h"
#include "graph.h"

struct PassThroughPair {

    // The input and output are in the same block.  and they share the
    // same ring buffer.  Hence, the data is not copied to another buffer
    // when passing through this block; it's passing through the block.
    struct QsInput *input;
    struct QsOutput *output;
};


static inline struct PassThroughPair
IsPassThrough(struct QsSimpleBlock *b, struct QsOutput *output) {

    struct PassThroughPair p;
    p.input = 0;
    p.output = 0;

    /// MORE HEREEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE

    // We pass a copy of a struct PassThroughPair back.
    return p;
}


static
void GetBuffer(struct QsBuffer *buffer,
        struct QsSimpleBlock *b,
        size_t mapLength, size_t overhangLength,
        // this output is an output in the block "b"
        struct QsOutput *output) {

    DASSERT(output->numInputs);
    DASSERT(output->inputs);

    if(!buffer) {
        buffer = calloc(1, sizeof(*buffer));
        ASSERT(buffer, "calloc(1,%zu) failed", sizeof(*buffer));
        output->buffer = buffer;
    }

    size_t maxRead = 0;

    for(uint32_t i = output->numInputs - 1; i != -1; --i) {
        if(maxRead < output->inputs[i]->maxRead)
            maxRead = output->inputs[i]->maxRead;
        output->inputs[i]->buffer = buffer;
    }
    // Add a reader length to the buffer mapping sizes.
    mapLength += maxRead;
    if(overhangLength < maxRead)
        overhangLength = maxRead;

    // TODO: If we allowed loops in the stream we'd have to change this
    // function.

    // There can only be at most one input that has pass-through write
    // access to a pass-through buffer per block output-to-inputs
    // connection stage.  There can be any number of connection
    // pass-through buffer stages.
    struct PassThroughPair p = IsPassThrough(b, output);
    if(p.input) {
        // We recurse.  This "input" is passing to a block downstream.
        DASSERT(p.input->block != b);
        DASSERT(p.input->feederBlock == b);
        // Add a writer length to the buffer mapping sizes.
        mapLength += p.output->maxWrite;
        if(overhangLength < p.output->maxWrite)
            overhangLength = p.output->maxWrite;

        // We will be passing data through this buffer in the block
        // that contains p.input and p.output.
        output->next = p.output;
        p.output->prev = output;
        p.output->buffer = buffer;
        p.input->buffer = buffer;

        GetBuffer(buffer, p.input->block, mapLength, 
                overhangLength, p.output);

        p.output->writePtr = buffer->end - buffer->mapLength;
        p.input->readPtr = p.output->writePtr;
        return;
    }

    // This buffer does not pass-through this block "b" and its' output.
    // This block "b" is at the end of a "pass-through" buffer chain.
    buffer->mapLength = mapLength;
    buffer->overhangLength = overhangLength;

    uint8_t *x =
        makeRingBuffer(&buffer->mapLength, &buffer->overhangLength);
    buffer->end = x + buffer->mapLength;


    for(uint32_t i = output->numInputs - 1; i != -1; --i) {
        output->inputs[i]->readPtr = x;
        struct QsSimpleBlock *smB = output->inputs[i]->block;

        for(uint32_t j = smB->numOutputs - 1; j != -1; --j) {
            struct QsOutput *output = smB->outputs + j;
            if(!output->buffer)
                GetBuffer(0, smB, output->maxWrite,
                        output->maxWrite, output);
        }
    }
}


void CreateRingBuffers(struct QsGraph *g) {

    DASSERT(g);
    DASSERT(g->sources);

    struct QsSimpleBlock **s = g->sources;

    for(struct QsSimpleBlock *b = *s; b; b = *(++s)) {
        DASSERT(b->outputs);
        DASSERT(b->numOutputs);
        for(uint32_t i = b->numOutputs - 1; i != -1; --i) {
            size_t maxWrite = (b->outputs + i)->maxWrite;
            GetBuffer(0, b, maxWrite, maxWrite, b->outputs + i);
        }
    } 
}


void DestroyRingBuffers(struct QsGraph *g) {

    DASSERT(g);
    DASSERT(g->sources);

    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
        for(uint32_t i = smB->numInputs - 1; i != -1; --i) {
            struct QsInput *input = smB->inputs + i;
            input->readPtr = 0;
            input->readLength = 0;
            input->buffer = 0;
        }
        for(uint32_t i = smB->numOutputs - 1; i != -1; --i) {
            struct QsOutput *output = smB->outputs + i;
            output->writePtr = 0;
            output->next = 0;
            if(output->prev == 0) {
                // We let this output own the buffer.
                struct QsBuffer *buffer = output->buffer;
                DASSERT(buffer);
#ifdef DEBUG
                memset(buffer->end - buffer->mapLength, 0,
                    buffer->mapLength);
#endif
                freeRingBuffer(buffer->end - buffer->mapLength,
                        buffer->mapLength, buffer->overhangLength);
#ifdef DEBUG
                memset(buffer, 0, sizeof(*buffer));
#endif
                free(buffer);
            } else
                output->prev = 0;

            output->buffer = 0;
        }
    }
}
