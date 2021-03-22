#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>

#include <X11/Xlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkx.h>

#include "../include/quickstream/app.h"
#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"

#include "../lib/debug.h"
#include "../lib/block.h"
#include "../lib/parameter.h"

#include "qsb.h"



static inline void GetConnectionColor(enum ConnectorKind ckind,
        double *r, double *g, double *b, double *a) {

    switch(ckind) {
        case Input:
            *r = 1.0;
            *g = 0.0;
            *b = 0.0;
            *a = 0.1;
            break;
        case Output:
            *r = 1.0;
            *g = 0.0;
            *b = 0.0;
            *a = 0.1;
            break;
        case Getter:
            *r = 0.0;
            *g = 0.0;
            *b = 1.0;
            *a = 0.1;
            break;
        case Setter:
            *r = 0.0;
            *g = 0.0;
            *b = 1.0;
            *a = 0.1;
        case Constant:
            *r = 0.0;
            *g = 1.0;
            *b = 1.0;
            *a = 0.1;
    }
}


/*
   Block with geo: ICOSG (Input,Constant,Output,SetterGetter)
   rotating round clock-wise.

 *******************************************
 *       |          const         |        |
 *       |************************|        |
 *       |        path.so         |        |
 * input |                        | output |
 *       |         name           |        |
 *       |************************|        |
 *       |    set    |     get    |        |
 *******************************************

 
 and there are 7 more permutations that we allow.  We keep the input and
 output on opposing sides and the parameters constant opposing setter and
 getter; with setter always before getter.  That gives 8 total orientation
 permutations that we allow.








 */

// Draw a glyph that looks like this circle arrow thingy:
//
//
//  -------> x
//  |
//  |
//  |     |                    |     
//  V     |<---- length ------>|
//  y     |                    |
//
//                        *
//            * *         * *
//          *     *       *   *
//         *        *******     *
//         *                     *
//         *        *******     *
//          *     *       *   *
//            * *         * *
//                        *
//
//
//
//
//  The height a small faction of the length
//
//  angle =  0  points right
//  angle =  90 points down
//  angle = -90 points up
//  angle = 180 points left
//
static void DrawArrowGlyph(cairo_t *cr, double x, double y,
        double length, double angle/*in degrees*/) {

    cairo_new_path(cr);
    cairo_translate(cr, x, y);
    if(angle)
        cairo_rotate(cr, angle*M_PI/180.0);
    cairo_arc(cr, -0.4*length, 0.0, 0.1*length,
            //90.0*M_PI/180.0, 270.0*M_PI/180.0);
            30.0*M_PI/180.0, 330.0*M_PI/180.0);
    cairo_rel_line_to(cr, 0.3*length, 0);
    double p1[2];
    cairo_get_current_point(cr, p1, p1+1);
    cairo_rel_line_to(cr, 0, - length/10.0); // y min
    cairo_line_to(cr, 0.5*length, 0.0); // arrow point
    cairo_line_to(cr, p1[0], length/10.0 + - p1[1]); // y max
    cairo_line_to(cr, p1[0], - p1[1]);
    cairo_close_path(cr); // implied
    if(angle)
        cairo_rotate(cr, -angle*M_PI/180.0);
    cairo_translate(cr, -x, -y);
    cairo_fill(cr);
}


static gboolean ConnectorDraw_CB(GtkWidget *widget,
        cairo_t *cr,
        struct Connector *c) {

    guint width, height;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context (widget);

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_render_background(context, cr, 0, 0, width, height);

    DASSERT(c);

    if(!c->active) return FALSE;

    ASSERT(!c->block->block->isSuperBlock,
            "Write this code to work with super blocks");

    // We will need a width or height of each strip.
    double delta;
    // The drawing fucks up without converting this to a double.
    // Dividing double by and int is a problem.
    double numPins = c->numPins;

    const enum ConnectorGeo geo = c->block->geo;

    //////////////////////////////////////////////////////////////
    // Draw strips that brake up the connectors into sections.
    // The sections are the pins in the connectors.
    //
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.07);
    //
    switch(geo) {
        case ICOSG:
        case OCISG:
        case ISGOC:
        case OSGIC:
            switch(c->kind) {
                case Setter:
                case Getter:
                case Constant:
                    // Connector is horizontal.
                    delta = width/numPins;
                    for(uint32_t i=1; i < c->numPins; i += 2)
                        cairo_rectangle(cr, i*delta, 0, delta, height);
                    break;
                case Input:
                case Output:
                    // Connector is vertical.
                    delta = height/numPins;
                    for(uint32_t i=1; i < c->numPins; i += 2)
                        cairo_rectangle(cr, 0, i*delta, width, delta);
                    break;
            }
            break;
        case COSGI:
        case CISGO:
        case SGOCI:
        case SGICO:
            switch(c->kind) {
                case Input:
                case Output:
                    // Connector is horizontal.
                    delta = width/numPins;
                    for(uint32_t i=1; i < c->numPins; i += 2)
                        cairo_rectangle(cr, i*delta, 0, delta, height);
                    break;
                case Setter:
                case Getter:
                case Constant:
                    // Connector is vertical.
                    delta = height/numPins;
                    for(uint32_t i=1; i < c->numPins; i += 2)
                        cairo_rectangle(cr, 0, i*delta, width, delta);
                    break;
            }
            break;
    }
    //
    cairo_fill(cr);



    //////////////////////////////////////////////////////////////////
    //     Now draw connector pins.  They are a dot with an arrow.
    //
    const double radius= ((double)CONNECTOR_THICKNESS)/8.0;
    const double length = CONNECTOR_THICKNESS*0.74;
    const double delta2 = delta/2.0;
    double direction; // angle in degrees

    // These could be one-liners, but the compiler will fuck-up the type
    // converted value in the one-liner.  Weird shit.
    double h2 = height;
    h2 /= 2.0;
    double w2 = width;
    w2 /= 2.0;
    //
    // Draw pin circles in the center of the strips that brake up the
    // connectors into sections.  The circles are the pins in the
    // connectors.
    //
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.3);
    //
    switch(geo) {
        case ICOSG:
        case OCISG:
        case ISGOC:
        case OSGIC:
            switch(c->kind) {
                case Setter:
                    // Connector is horizontal
                    if(geo == ISGOC || geo == OSGIC)
                        direction = 90.0; // pointing down
                    else
                        direction = -90.0; // pointing up
                    for(uint32_t i = 0; i < c->numPins; ++i)
                        DrawArrowGlyph(cr, i*delta + delta2, h2,
                                length, direction);
                    break;
                case Getter:
                    // Connector is horizontal
                    if(geo == ISGOC || geo == OSGIC)
                        direction = -90.0; // pointing up
                    else
                        direction = 90.0; // pointing down
                    for(uint32_t i = 0; i < c->numPins; ++i)
                        DrawArrowGlyph(cr, i*delta + delta2, h2,
                                length, direction);
                    break;
                case Constant:
                    // Connector is horizontal with no direction.
                    for(uint32_t i = 0; i < c->numPins; ++i)
                        cairo_arc(cr, i*delta + delta2, h2,
                                radius, 0, 2.0*M_PI);
                    cairo_fill(cr);
                    break;
                case Input:
                case Output:
                {
                    // Connector is vertical with direction right or left.
                    // Red connectors are required to be connected.
                    // Black connectors are not required to be connected.
                    uint32_t i = 0;
                    uint32_t n;
                    if(c->kind == Input) {
                        if(geo == ICOSG || geo == ISGOC)
                            // input is on the left side of the block.
                            direction = 0.0; // points to the right
                        else
                            // input is on the right side of the block.
                            direction = 180.0; // points to the left
                        n = c->block->block->minNumInputs;
                    } else {
                        if(geo == OCISG || geo == OSGIC)
                            // output is on the left side of the block.
                            direction = 180.0; // points to the left
                        else
                            // output is on the right side of the block.
                            direction = 0.0; // points to the right
                        n = c->block->block->minNumOutputs;
                    }
                    if(n)
                        // We're going red now.
                        cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.9);
                    for(; i < n; ++i)
                        DrawArrowGlyph(cr, w2, i*delta + delta2,
                                length, direction);
                    if(i < c->numPins) {
                        // Yes I'm Back In Black. (AC/DC)
                        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.3);
                    }
                    for(; i < c->numPins; ++i)
                        DrawArrowGlyph(cr, w2, i*delta + delta2,
                                length, direction);
                    break;
                }
            }
            break;
        case COSGI:
        case CISGO:
        case SGOCI:
        case SGICO:
            switch(c->kind) {
                case Setter:
                    // Connector is vertical
                    if(geo == SGOCI || geo == SGICO)
                        direction = 0.0; // pointing to the right
                    else
                        direction = 180.0; // pointing to the left
                    for(uint32_t i = 0; i < c->numPins; ++i)
                        DrawArrowGlyph(cr, w2, i*delta + delta2,
                                length, direction);
                    break;
                case Getter:
                    // Connector is vertical
                    if(geo == SGOCI || geo == SGICO)
                        direction = 180.0; // pointing to the left
                    else
                        direction = 0.0; // pointing to the right
                    for(uint32_t i = 0; i < c->numPins; ++i)
                        DrawArrowGlyph(cr, w2, i*delta + delta2,
                                length, direction);
                    break;
                case Constant:
                    // Connector is vertical with no direction.
                    for(uint32_t i = 0; i < c->numPins; ++i)
                        cairo_arc(cr, w2, i*delta + delta2,
                                radius, 0, 2.0*M_PI);
                    cairo_fill(cr);
                    break;
                case Input:
                case Output:
                {
                    // Connector is horizontal with direction up or down.
                    // Red connectors are required to be connected.
                    // Black connectors are not required to be connected.
                    uint32_t i = 0;
                    uint32_t n;
                    if(c->kind == Input) {
                        if(geo == CISGO || geo == SGICO)
                            // input is on the top side of the block.
                            direction = 90.0; // points down
                        else
                            // input is on the bottom side of the block.
                            direction = -90.0; // points up
                        n = c->block->block->minNumInputs;
                    } else { // Output
                        if(geo == COSGI || geo == SGOCI)
                            // output is on the top side of the block.
                            direction = -90.0; // points up
                        else
                            // output is on the botton side of the block.
                            direction = 90.0; // points down
                        n = c->block->block->minNumOutputs;
                    }
                    if(n)
                        // We're going red now.
                        cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.9);
                    for(; i < n; ++i)
                        DrawArrowGlyph(cr, i*delta + delta2, h2,
                                length, direction);
                    if(i < c->numPins) {
                        // Yes I'm Back In Black. (AC/DC)
                        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.3);
                    }
                    for(; i < c->numPins; ++i)
                        DrawArrowGlyph(cr, i*delta + delta2, h2,
                                length, direction);
                    break;
                }
                break;
            }
    }


    return FALSE;
}


#if 0
// Here is the complete list of possible kinds of parameters connections:
//
//   1. Getter    to  Setter
//   2. Constant  to  Setter
//   3. Constant  to  Constant
//
static
int CanConnectParametersCB(struct QsParameter *p,
            enum QsParameterKind kind,
            enum QsParameterType type,
            size_t size,
            const char *pName, struct QsParameter **connectTo) {

    DASSERT(p);
    DASSERT(p->kind == kind);
    DASSERT(p->type == type);
    DASSERT(p->size == size);
    DASSERT(*connectTo);

    struct QsParameter *P = *connectTo;

    DASSERT(P->type == type);
    DASSERT(P->size == size);

    if(kind == QsSetter && p->first)
        // A setter cannot be connected more than once.
        return 0;
    if(P->kind == QsSetter && P->first)
        // A setter cannot be connected more than once.
        return 0;


    if(
        (P->kind == QsGetter   &&    kind == QsSetter) ||
        (   kind == QsGetter   && P->kind == QsSetter) ||
        (P->kind == QsConstant &&    kind == QsSetter) ||
        (   kind == QsConstant && P->kind == QsSetter) ||
        (P->kind == QsConstant &&    kind == QsConstant)) {
        // We can connect these parameters?  We use *connectTo = 0
        // as a flag that answers yes to that question.
        *connectTo = 0;
        return 1; // Stop calling the this function.
    }

    // else we cannot connect these parameters.
    return 0;
}


// Return the Answer the Question: Is a Connection Possible?
//
// The from connector, "from", is already selected, but this is deciding
// whither or not we can connect from "from" to connector "to".  If not
// the user will not see it as an option.
//
static
bool CheckConnectionPossible(struct Connector *from,
        struct Connector *to) {

    DASSERT(from);
    DASSERT(to);

    DASSERT(!from->block->block->isSuperBlock);
    DASSERT(!to->block->block->isSuperBlock);


    switch(from->kind) {
        case Input:
            if(to->kind != Output)
                return false;
            // Connecting from a Input to an Output.
            //
            // Since each output port may have any number of
            // connections from input ports, we will show all output
            // ports in the connector popup menu.
            return true;
        case Output:
        {
            if(to->kind != Input)
                return false;
            // Connecting from a Output to an Input.
            //
            // There must be an empty input port or we can't connect.
            struct QsSimpleBlock *smB =
                (struct QsSimpleBlock *) to->block->block;
            if(smB->numInputs < smB->block.maxNumInputs)
                // We are not using all possible input ports, so we go
                // to the next step.
                return true;
            // But we could have unconnected port before the highest
            // port, so we check for that now.
            uint32_t i=smB->numInputs-1;
            for(;i!=-1; --i)
                if(!smB->inputs[i].block)
                    // this input port i is not connected.
                    return true;
            if(i == -1)
                // We searched all ports and all are in use.
                return false;
            // There is an input port not in use.
            return true;
        }
        case Setter:
        case Getter:
        case Constant:
        {
            // Now we need to see if there are any parameters that
            // this "from" blocks' parameter can connect to.
            struct QsParameter *fromParameter = from->parameter;

            qsParameterForEach(0, to->block->block,
                QsAny/*kind*/,
                from->parameter->type,
                from->parameter->size,
                0/*name 0=any*/,
                (int (*)(struct QsParameter *p,
                    enum QsParameterKind kind,
                    enum QsParameterType type,
                    size_t size,
                    const char *pName, void *))
                    CanConnectParametersCB/*callback*/,
                    &fromParameter/*userData*/, 0/*flags*/);
            return fromParameter?false:true;
        }
    }

    DASSERT(0, "We should not get here");
    return true;
}


// This just answers the question: Is a connection possible "from" this
// connector?  We do not know the other, "to", connector yet.
//
// TODO: This does not bother looking at all the other blocks, to make
// sure that there is a compatible "to" connector available.  Doing so
// may make the user interface a little confusing.
bool
CheckConnectionFromPossible(struct Connector *c) {

    struct QsBlock *b = c->block->block;
    ASSERT(!b->isSuperBlock);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    switch(c->kind) {
        case Input:
        {
            // Inputs can only be connected to once, so if there
            // is an unused input port available, then we may be
            // able to connect to it.
            //
            for(uint32_t i = b->maxNumInputs; i != -1; --i) {
                if(i >= smB->numInputs)
                    // There is an unused input port possible.
                    return true;
                if(smB->inputs[i].block == 0)
                    // There is an unused input port possible.
                    return true;
                // This input port, i, is in use.
            }
        }
        break;
        case Output:
        {
            // MORE CODE HERE...
            return false;
        }
        break;
        case Getter:
        case Setter:
        case Constant:
        {
            // MORE CODE HERE...
            return false;
        }
        break;
    }

    ASSERT(0, "We should not get here");
    return false;
}
#endif


// When fromConnector is set we are dragging a connection line.

// When this is set we are doing "connector pin hovering"
// on the layout widget.
static struct Popover *connectorsPopover = 0;
// The popover label that we change as the mouse pointer moves over the
// different connector pins.
static GtkWidget *currentWidget = 0;
static uint32_t currentPinNum = -1;
static bool popoverShowing = false;


static void ShowPinBalloon(GtkWidget *draw, GdkEventButton *e,
        struct Connector *c) {

    if(!connectorsPopover)
        // Somehow we get motion events after we leave the widget.
        return;


    if(movingBlock) {
        if(popoverShowing) {
            gtk_widget_hide(connectorsPopover->container);
            popoverShowing = false;
        }
        return;
    }


    DASSERT(c);
    DASSERT(draw == c->widget);
    DASSERT(connectorsPopover);
    double numPins = c->numPins;
    DASSERT(numPins);

    // Check the current pin number that the pointer is hovering over.
    uint32_t pinNum;

    double pos[2];
    double layoutPos[2];
    double p;
    double x, y;
    double delta;
    GetWidgetRootXY(draw, pos, pos+1);
    GetWidgetRootXY(c->block->page->layout, layoutPos, layoutPos+1);

    double width = gtk_widget_get_allocated_width(draw);
    double height = gtk_widget_get_allocated_height(draw);

    if(c->isHorizontal) {
        // It's oriented horizontally; so the pin number depends on the x
        // position of the pointer.
        delta = e->x_root - pos[0];
        p = numPins*delta/width;
        if(p < 0.0) pinNum = 0;
        else if(p >= numPins) pinNum = numPins - 1;
        else pinNum = p;
        x = pos[0] + pinNum*(width/numPins) - layoutPos[0];
        y = (pos[1] + height) - layoutPos[1];
    } else {
        // It's oriented vertically, so the pin number depends on the y
        // position of the pointer.
        delta = e->y_root - pos[1];
        p = numPins*delta/height;
        if(p < 0.0) pinNum = 0;
        else if(p >= numPins) pinNum = numPins - 1;
        else pinNum = p;
        x = pos[0] + width - layoutPos[0];
        y = pos[1] + pinNum*(height/numPins) - layoutPos[1];
        //delta = height/numPins;
        //pos.y = pinNum*delta + delta/2.0;
        //pos.x = CONNECTOR_THICKNESS/2;
    }


    if(currentWidget != draw) {
        // Act like the pin number changed, because it's a different set
        // of pins, and it's a change even if the pin number is the same.
        currentPinNum = -1;
    }

    // If the pin is not changed we do not need to move the popover.
    if(currentPinNum != pinNum) {
        // The pin number changed.

        char text[64];
        snprintf(text, 64, "pin number %" PRIu32 "  %g", pinNum, p);
        gtk_label_set_text(GTK_LABEL(connectorsPopover->label), text);
        
        if(currentWidget != draw) {
            // Popup the Popover and move it.
            g_object_ref(G_OBJECT(connectorsPopover->container));

            // remove the widget which removes one reference
            gtk_container_remove(GTK_CONTAINER(c->block->page->layout),
                    connectorsPopover->container);
            // Now we have at least one reference.
            // re-add the widget which will take ownership of the
            // one remaining reference.
            gtk_layout_put(GTK_LAYOUT(c->block->page->layout),
                    connectorsPopover->container, x, y);
            currentWidget = draw;
        } else
            // Just move the Popover
            gtk_layout_move(GTK_LAYOUT(c->block->page->layout),
                    connectorsPopover->container, x, y);

        currentPinNum = pinNum;
    }
}



static gboolean ConnectorMotion_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {
WARN();
    ShowPinBalloon(draw, e, c);

    return FALSE; // TRUE = eat event
}


static gboolean ConnectorEnter_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {

    DASSERT(c);
    DASSERT(c->numPins);
    DASSERT(draw == c->widget);
WARN();
    if(!connectorsPopover) {
        // GTK+3 does not keep the order of events in a sensible way.
        // We sometimes get 2 enter events with no leave event.
        connectorsPopover = &c->block->page->connectorsPopover;
    }

    if(movingBlock) {
        if(popoverShowing) {
            gtk_widget_hide(connectorsPopover->container);
            popoverShowing = false;
        }
        return FALSE; // TRUE = eat event
    }


    gtk_widget_show(connectorsPopover->container);
    ShowPinBalloon(draw, e, c);

    return FALSE; // TRUE = eat event
}


static gboolean ConnectorLeave_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {

    DASSERT(c);
    DASSERT(c->numPins);
    DASSERT(draw == c->widget);
    //DASSERT(connectorsPopover);
WARN();

    if(connectorsPopover) {

        gtk_widget_hide(connectorsPopover->container);
        popoverShowing = false;
        connectorsPopover = 0;
        currentPinNum = -1;
        currentWidget = 0;
    }

    return FALSE; // TRUE = eat event
}


static gboolean ConnectorRelease_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {


    DASSERT(draw == c->widget);
    return FALSE; // TRUE = eat event
}


// The connector is a GTK drawingArea widget, "draw".
static gboolean ConnectorPress_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {

    DASSERT(c);
    DASSERT(draw == c->widget);
    DASSERT(c->active);
    ASSERT(c->block->block->isSuperBlock == 0, "Write this code");


    if(e->button == CONNECT_BUTTON) {
        // Event goes to next parent widget.

        return FALSE; // pass the event to the parent block widget.
    }


    return FALSE; // TRUE = eat this event
}


void MakeBlockConnector(GtkWidget *grid,
        const char *className/*for CSS*/,
        enum ConnectorKind ckind,
        struct Block *block) {

    GtkWidget *drawArea = gtk_drawing_area_new();

    gtk_widget_set_can_focus(drawArea, TRUE);
    gtk_widget_add_events(drawArea,
            GDK_ENTER_NOTIFY_MASK|
            GDK_LEAVE_NOTIFY_MASK |
            GDK_SCROLL_MASK |
            GDK_POINTER_MOTION_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_BUTTON_PRESS_MASK|
            GDK_STRUCTURE_MASK);

    ASSERT(!block->block->isSuperBlock, "Write more code HERE");
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) block->block;
    
    gtk_widget_show(drawArea);
    gtk_widget_set_size_request(drawArea, CONNECTOR_THICKNESS,
            CONNECTOR_THICKNESS);
    gtk_widget_set_name(drawArea, className);

    struct Connector *c;

    switch(ckind) {
        case Constant:
            c = &block->constants;
            c->numPins = smB->numConstants;
            break;
        case Getter:
            c = &block->getters;
            c->numPins = smB->numGetters;
            break;
        case Setter:
            c = &block->setters;
            c->numPins = smB->numSetters;
            break;
        case Input:
            c = &block->input;
            c->numPins = block->block->maxNumInputs;
            break;
        case Output:
            c = &block->output;
            c->numPins = block->block->maxNumOutputs;
            break;
    }

    c->kind = ckind;
    c->widget = drawArea;
    c->block = block;
    snprintf(c->name, CONNECTOR_CLASSNAME_LEN, "%s", className);

    g_signal_connect(G_OBJECT(drawArea), "draw",
            G_CALLBACK(ConnectorDraw_CB), c/*userData*/);

    c->active = (c->numPins)?true:false;

    if(!c->active)
        // If the connector is not able to make connections we do not need
        // the next few callbacks setup.
        return;

    g_signal_connect(GTK_WIDGET(drawArea), "motion-notify-event",
            G_CALLBACK(ConnectorMotion_CB), c/*userData*/);
    g_signal_connect(GTK_WIDGET(drawArea), "enter-notify-event",
            G_CALLBACK(ConnectorEnter_CB), c/*userData*/);
    g_signal_connect(GTK_WIDGET(drawArea), "leave-notify-event",
            G_CALLBACK(ConnectorLeave_CB), c/*userData*/);
    g_signal_connect(GTK_WIDGET(drawArea), "button-release-event",
            G_CALLBACK(ConnectorRelease_CB), c/*userData*/);
    g_signal_connect(GTK_WIDGET(drawArea), "button-press-event",
            G_CALLBACK(ConnectorPress_CB), c/*userData*/);
}
