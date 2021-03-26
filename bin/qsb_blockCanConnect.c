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
            // else it's in the inputs[] array with no connection yet.
            return true;

        case Output:
            // Outputs can have any amount of connections.
            return true;

    // MORE CODE HERE
        case Constant:
        case Setter:
        case Getter:
            return true;
    }
    ASSERT(0, "We should not get here");
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

    struct Connector *c1 = pin1->connector;
    DASSERT(c1);
    struct Connector *c2 = pin2->connector;
    DASSERT(c2);
    DASSERT(c1 != c2);

    switch(c1->kind) {
        case Input:
        case Output:
            if((c1->kind == Input && c2->kind == Output &&
                    CanConnectFromPin(pin1)) ||
                    (c2->kind == Input && c1->kind == Output &&
                     CanConnectFromPin(pin2))) {
                return true;
            } else {
                return false;
            }
        case Constant:
        case Setter:
        case Getter:

            break;
    }

    return false;
}


// From the prospective of this function the pin1 and pin2 can be
// interchanged.
//
// CanConnect2Pins() must be called before this.
//
void Connect2Pins(struct Pin *pin1, struct Pin *pin2) {

    // It should have been verified that we can connect from the "from"
    // pin, irregardless of what the "to" pin is.
    DASSERT(CanConnectFromPin(pin1));
    DASSERT(CanConnectFromPin(pin2));
    DASSERT(CanConnect2Pins(pin1, pin2));

    struct Connector *c1 = pin1->connector;
    struct Connector *c2 = pin2->connector;

    switch(c1->kind) {
        case Input:
        case Output:
            // We already checked that the input port is not occupied and
            // that the other port is an output.
            qsBlockConnect(c1->block->block, c2->block->block,
                    pin1->portNum, pin2->portNum);
            return;
        case Constant:
        case Setter:
        case Getter:
            break;
    }

    struct Page *page = c1->block->page;
    DASSERT(page);

    DrawConnection(page, pin1, pin2);

    // This gets done when we stop un-draw the connection drag line.
    //gtk_widget_queue_draw_area(page->layout, 0, 0, page->w, page->h);
}

