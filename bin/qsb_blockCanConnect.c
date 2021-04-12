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
            if(pin->portNum >= b->numInputs)
                return true;
            // So now pin->portNum < b->numInputs
            if(b->inputs[pin->portNum].block)
                return false;
            // else it's in the inputs[] array with no connection yet.
            return true;
        case Output:
            // Outputs can have any amount of connections.
            return true;
        case Setter:
        case Constant:
        case Getter:
            // No connection restriction yet.
            return true;
    }

    DASSERT(0, "We should not get here");
    return false;
}


// From the prospective of this function the pin1 and pin2 can be
// interchanged.
//
bool CanConnect2Pins(struct Pin *pin1, struct Pin *pin2) {

    // It should have been verified that we can connect from the "from"
    // pin, irregardless of what the "to" pin is.
    DASSERT(CanConnectFromPin(pin1));
    DASSERT(CanConnectFromPin(pin2));

    if(pin1 == pin2)
        return false;

    struct Connector *c1 = pin1->connector;
    DASSERT(c1);
    struct Connector *c2 = pin2->connector;
    DASSERT(c2);

    switch(c1->kind) {
        case Input:
            if(c2->kind == Output)
                return true;
            return false;
        case Output:
            if(c2->kind == Input)
                return true;
            return false;

        // TODO: Is there a permutation math that could do this switching
        // in a simpler form?

        case Setter:
            if(c2->kind == Input || c2->kind == Output)
                return false;
            // Setters can connect to a constant group or a getter group.
            // We just can't connect them if 1. they are already connected
            // or 2. they belong to the two different group types.
            if(pin1->parameter->first &&
                    pin1->parameter->first == pin2->parameter->first)
                // They are connected already.
                return false;
            if(pin1->parameter->type != pin2->parameter->type ||
                    pin1->parameter->size != pin2->parameter->size)
                return false;
            if(!pin1->parameter->first || !pin2->parameter->first)
                // Setter that are not in a group yet can connect to any
                // parameter, assuming they are not connected yet.
                return true;
            if(pin1->parameter->first->kind == QsSetter ||
                    pin2->parameter->first->kind == QsSetter)
                // A setter or a setter group can connect to any type of
                // parameter group.
                return true;
            // At this point both pins are in a group that is either a
            // getter group or a constant group.  Two getter groups cannot
            // be connected.  A getter group cannot connect to a constant
            // group.  So:
            if(pin1->parameter->first->kind == QsGetter ||
                    pin2->parameter->first->kind == QsGetter)
                // We cannot connect a getter or constant group to another
                // getter group.
                return false;
            // All remaining cases a setter, pin1, can connect to.
            return true;
        case Constant:
            if(c2->kind == Input || c2->kind == Output)
                return false;
            if(pin1->parameter->type != pin2->parameter->type ||
                    pin1->parameter->size != pin2->parameter->size)
                return false;
            if(c2->kind == Getter ||
                    (pin2->parameter->first &&
                     pin2->parameter->first->kind == QsGetter))
                // A constant cannot connect to a getter or a getter
                // group.
                return false;
            // c1 can connect to a setter or another constant so long as
            // it is not connected to it already.
            if(pin1->parameter->first && pin2->parameter->first &&
                pin1->parameter->first == pin2->parameter->first)
                // These parameters share the same connection list,
                // therefore they are already connected.
                return false;
            // All remaining cases are connectable.
            return true;
        case Getter:
            if(c2->kind == Input || c2->kind == Output)
                return false;
            if(pin1->parameter->type != pin2->parameter->type ||
                    pin1->parameter->size != pin2->parameter->size)
                return false;
            // Getters only connect to setters.
            if(c2->kind == Setter) {
                if(pin1->parameter->first && pin2->parameter->first &&
                        pin1->parameter->first == pin2->parameter->first)
                    // These parameters share the same connection list,
                    // therefore they are already connected.
                    return false;
                return true;
            }
            return false;
    }

    DASSERT(0, "We should not get here");
    return false;
}


// From the prospective of this function the pin1 and pin2 can be
// interchanged.
//
// CanConnect2Pins() must be called before this.
//
void Connect2Pins(struct Pin *pin1, struct Pin *pin2) {

    // It should have been verified already.
    DASSERT(CanConnect2Pins(pin1, pin2));

    struct Connector *c1 = pin1->connector;
    struct Connector *c2 = pin2->connector;

    switch(c1->kind) {
        case Input:
            // We already checked that the input port is not occupied and
            // that the other port is an output.
            DASSERT(c2->kind == Output);
            // qsBlockConnect(BlockOutput, BlockInput,
            //             outputPort, inputPort)
            ASSERT(qsBlockConnect(c2->block->block, c1->block->block,
                    pin2->portNum, pin1->portNum) == 0);
            // Now we keep a record in this non-libquickstream code, so
            // we do not need to search for the output ports later.
            DASSERT(c2->kind == Output);
            DASSERT(!c1->block->inputConnections[pin1->portNum]);
            c1->block->inputConnections[pin1->portNum] = pin2;
            break;
        case Output:
            // We already checked that the input port is not occupied and
            // that the other port is an output.
            DASSERT(c2->kind == Input);
            // qsBlockConnect(BlockOutput, BlockInput,
            //             outputPort, inputPort)
            ASSERT(qsBlockConnect(c1->block->block, c2->block->block,
                    pin1->portNum, pin2->portNum) == 0);
            // Now we keep a record in this non-libquickstream code, so
            // we do not need to search for the output ports later.
            DASSERT(!c2->block->inputConnections[pin2->portNum]);
            c2->block->inputConnections[pin2->portNum] = pin1;
            break;
        case Constant:
        case Setter:
        case Getter:
            ASSERT(qsParameterConnect(pin1->parameter, pin2->parameter)
                    == 0);
            // Now add the connection to pin2 in the pin1 list.
            ++pin1->numTo;
            pin1->to = realloc(pin1->to, pin1->numTo*sizeof(*pin1->to));
            ASSERT(pin1->to, "reallloc(,%zu) failed",
                    pin1->numTo*sizeof(*pin1->to));
            pin1->to[pin1->numTo-1] = pin2;
            break;
    }

    struct Page *page = c1->block->page;
    DASSERT(page);

    DrawConnection(page, pin1, pin2);

    // Queuing the draw gets done when we stop un-draw the connection drag
    // line.
    //gtk_widget_queue_draw_area(page->layout, 0, 0, page->w, page->h);
}
