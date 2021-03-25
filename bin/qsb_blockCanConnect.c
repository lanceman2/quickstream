#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>

#include <gtk/gtk.h>

#include "../include/quickstream/app.h"
#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"

#include "../lib/debug.h"
#include "../lib/block.h"
#include "../lib/parameter.h"

#include "qsb.h"



bool CanConnectFromPin(struct Pin *pin) {

    DASSERT(pin);
    struct Connector *c = pin->connector;
    DASSERT(c);
 
    ASSERT(!c->block->block->isSuperBlock, "Write code for super blocks");
 
    struct QsSimpleBlock *b = (struct QsSimpleBlock *) c->block->block;
 
    switch(c->kind) {

        case Input:
            DASSERT(c->numPins == c->block->block->maxNumInputs);
            if(pin->portNum + 1 > b->numInputs)
                return true;
            if(b->inputs[pin->portNum].block)
                return false;
            return true;

    // MORE CODE HERE
        case Output:
        case Constant:
        case Setter:
        case Getter:
            return true;
    }
    ASSERT(0, "We should not get here");
    return false;
}
