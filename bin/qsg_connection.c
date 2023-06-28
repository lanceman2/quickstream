#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "../include/quickstream.h"

#include "quickstreamGUI.h"



static inline
void RemoveConnectionElement(struct Connection *c,
            struct ConnectionArray *cons) {

    DASSERT(c);
    DASSERT(cons);
    // Remove the connection, c, from the cons connection list:
    DASSERT(cons->numConnections);
    DASSERT(cons->connections);
    uint32_t i = 0;
    for(; i < cons->numConnections; ++i)
        if(cons->connections[i] == c)
            // We found the pointer pointer.
            break;
    DASSERT(i < cons->numConnections);
    --cons->numConnections;
    for(; i < cons->numConnections; ++i)
        cons->connections[i] = cons->connections[i+1];
#ifdef DEBUG
    // Zero the last one.
    cons->connections[i] = 0;
#endif
    if(cons->numConnections) {
        // Squish the array by one.
        cons->connections = realloc(cons->connections,
                cons->numConnections*sizeof(*cons->connections));
        ASSERT(cons->connections, "realloc(,%zu) failed",
                cons->numConnections*sizeof(*cons->connections));
    } else {
        free(cons->connections);
        cons->connections = 0;
    }
}


static
void RemoveConnection(struct Connection *c) {

    DASSERT(c);
    struct Port *p1 = c->port1;
    DASSERT(p1);
    struct Port *p2 = c->port2;
    DASSERT(p2);
    DASSERT(p1->terminal);
    DASSERT(p2->terminal);
    struct Block *b1 = p1->terminal->block;
    DASSERT(b1);
    struct Block *b2 = p2->terminal->block;
    DASSERT(b2);
    struct Layout *l = b1->layout;
    DASSERT(l);
    DASSERT(l == b2->layout);
    DASSERT(b1->qsBlock);
    DASSERT(b2->qsBlock);
    DASSERT(l->qsGraph);

    // Remove the connection, c, from the layout connection list:
    DASSERT(l->firstConnection);
    DASSERT(l->lastConnection);
    if(c->prev) {
        DASSERT(c != l->firstConnection);
        c->prev->next = c->next;
    } else {
        DASSERT(c == l->firstConnection);
        l->firstConnection = c->next;
    }
    if(c->next) {
        DASSERT(c != l->lastConnection);
        c->next->prev = c->prev;
        c->next = 0;
    } else {
        DASSERT(c == l->lastConnection);
        l->lastConnection = c->prev;
    }
    c->prev = 0;

    // Remove the connection, c, from the p1 connections list:
    RemoveConnectionElement(c, &p1->cons);

    // Remove the connection, c, from the p2 connections list:
    RemoveConnectionElement(c, &p2->cons);

    // Remove the connection, c, from the b1 connections list:
    RemoveConnectionElement(c, &b1->cons);

    // Remove the connection, c, from the b2 connections list:
    RemoveConnectionElement(c, &b2->cons);


    l->surfaceNeedRedraw = true;
    gtk_widget_queue_draw(l->layout);
}


void Disconnect(struct Port *p) {

    DASSERT(p);
    DASSERT(p->qsPort);
    DASSERT(p->terminal);
    DASSERT(p->cons.numConnections);

    ASSERT(0 == qsGraph_disconnect(p->qsPort));

    // For control parameter ports the connect in the libquickstream.so
    // API is a connection to a group of ports (not just one port to
    // another port), though we show it graphically like a electrically
    // conducting line without fully connecting all nodes in the group.
    // So we need this "conductor" to keep the ports in the group
    // connected to one of the ports in the group.

    if((p->terminal->type == Set || p->terminal->type == Get) &&
            // There must be more than one other port connecting to
            // port, p, for it to need a replacement hub port.
            p->cons.numConnections > 1) {
        // We need to graphically connect the other ports in the group
        // for parameter connections.
        struct Port *p1;
        if((*p->cons.connections)->port1 != p) {
            DASSERT((*p->cons.connections)->port2 == p);
            p1 = (*p->cons.connections)->port1;
        } else {
            DASSERT((*p->cons.connections)->port2 != p);
            p1 = (*p->cons.connections)->port2;
        }
        // We display the p1 port as a central connector port.  This
        // choose of central port is somewhat arbitrary.

        for(uint32_t i = p->cons.numConnections - 1; i != 0; --i) {
            struct Connection *c = p->cons.connections[i];
            if(c->port1 == p) {
                DASSERT(c->port2 != p);
                AddConnection(p1, c->port2, false);
            } else {
                DASSERT(c->port2 == p);
                AddConnection(p1, c->port1, false);
            }
        }
    }

    while(p->cons.numConnections)
        RemoveConnection(p->cons.connections[p->cons.numConnections-1]);

    DASSERT(!p->cons.connections);
}


// Returns x,y in layout coordinates where to draw the connection line to
// or from (either end point).  The blocks and the terminals move around
// so we will keep calling this.  The terminals move relative to the block
// too, not just with it, and not just due to a block rotation either.
void GetPortConnectionPoint(double *x, double *y, const struct Port *p) {

    DASSERT(x);
    DASSERT(y);
    DASSERT(p);
    struct Terminal *t = p->terminal;
    DASSERT(t);

    GtkAllocation a;
    DASSERT(t->block);
    DASSERT(t->block->grid);
    gtk_widget_get_allocation(t->block->grid, &a);

    // Get the distance along the long way to the port.  That could be up
    // to down, or left to right; depending on the terminal orientation.
    double d = PortIndexToPosition(t, p->num);

    // Note: we put the connection point under the block border,
    // at the outer edge of the drawing area of the terminal; and
    // centered on the port pin.
    //
    // switch on the side the terminal is on.
    switch(t->pos) {
        case Left:
            *x = t->block->x + BW;
            *y = t->block->y + BW + d;
            break;
        case Right:
            *x = t->block->x + a.width - BW;
            *y = t->block->y + BW + d;
            break;
        case Top:
            *x = t->block->x + BW + CON_THICKNESS + d;
            *y = t->block->y + BW;
            break;
        case Bottom:
            *x = t->block->x + BW + CON_THICKNESS + d;
            *y = t->block->y + a.height - BW;
            break;
        case NumPos:
            // We do not handle the null orientation.
            ASSERT(0);
    }
}


static inline
void AddConnectionElement(struct Connection *c,
        struct ConnectionArray *cons) {

    DASSERT(c);
    DASSERT(cons);

    ++cons->numConnections;
    DASSERT(cons->numConnections);
    cons->connections = realloc(cons->connections,
            cons->numConnections*sizeof(*cons->connections));
    ASSERT(cons->connections, "realloc(,%zu) failed",
            cons->numConnections*sizeof(*cons->connections));
    cons->connections[cons->numConnections-1] = c;
}

// If qsConnect is not true then this will not call the libquickstream API
// to make the connection; it will just do the GUI stuff; else it will not
// call the libquickstream API to make the connection (we assume it's done
// already).
//
void AddConnection(struct Port *p1, struct Port *p2,
        bool qsConnect) {

    DASSERT(p1);
    DASSERT(p2);
    DASSERT(p1->terminal);
    DASSERT(p2->terminal);
    struct Block *b1 = p1->terminal->block;
    DASSERT(b1);
    struct Block *b2 = p2->terminal->block;
    DASSERT(b2);
    struct Layout *l = b1->layout;
    DASSERT(l);
    DASSERT(l == b2->layout);
    DASSERT(b1->qsBlock);
    DASSERT(b2->qsBlock);
    DASSERT(l->qsGraph);

    // We assume they are not connected already.

    struct Connection *c = calloc(1, sizeof(*c));
    ASSERT(c, "calloc(1,%zu) failed", sizeof(*c));
    c->port1 = p1;
    c->port2 = p2;
    
    // Add the connection, c, to the layout connection list:
    if(l->firstConnection) {
        DASSERT(l->lastConnection);
        c->prev = l->lastConnection;
        l->lastConnection->next = c;
    } else {
        DASSERT(!l->lastConnection);
        l->firstConnection = c;
    }
    l->lastConnection = c;

    // Add the connection, c, to the p1 connection list:
    AddConnectionElement(c, &p1->cons);

    // Add the connection, c, to the p2 connection list:
    AddConnectionElement(c, &p2->cons);

    // Add the connection, c, to the p1 block connection list:
    AddConnectionElement(c, &b1->cons);

    // Add the connection, c, to the p1 block connection list:
    AddConnectionElement(c, &b2->cons);

    if(qsConnect)
        ASSERT(0 == qsGraph_connect(p1->qsPort, p2->qsPort));

    l->surfaceNeedRedraw = true;
    gtk_widget_queue_draw(l->layout);
}


