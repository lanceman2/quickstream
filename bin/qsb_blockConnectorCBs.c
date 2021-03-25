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



//XUngrabPointer



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


gboolean ConnectorDraw_CB(GtkWidget *widget,
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



// x_root, y_root are the position of the mouse pointer in the root
// window.   The only sure reference frame is the root window frame, all
// other frames are not reliable, things move and event positions are
// relative to the first widget that may be letting the event get passed
// to a descendent widget.
//
// TODO: Maybe most of this can be moved to the OrientConnectors functions.
//
static struct Pin *GetConnectorPinAndPosition(GtkWidget *draw, double x_root,
        double y_root, struct Connector *c) {

    struct Pin *pin;

    // I can't believe this takes this many variables.
    uint32_t pinNum;
    double pos[2];
    double layoutPos[2];
    double p;
    double delta;
    double numPins = c->numPins;
    GetWidgetRootXY(draw, pos, pos+1);
    GetWidgetRootXY(c->block->page->layout, layoutPos, layoutPos+1);
    double width = gtk_widget_get_allocated_width(draw);
    double height = gtk_widget_get_allocated_height(draw);

    if(c->isHorizontal) {
        // The connector is oriented horizontally; so the pin number
        // depends on the x position of the pointer.
        delta = x_root - pos[0];
        p = numPins*delta/width;
        if(p < 0.0) pinNum = 0;
        else if(p >= numPins) pinNum = numPins - 1;
        else pinNum = p;
        pin = c->pins + pinNum;
        pin->connector->x = pos[0] + (pinNum+0.5)*(width/numPins) -
                layoutPos[0];
        pin->connector->y = (pos[1] + height/2.0) - layoutPos[1];
        if(pin->connector->isSouthWestOfBlock) {
            pin->connector->dx = 0.0;
            pin->connector->dy = 1.0;
        } else {
            pin->connector->dx = 0.0;
            pin->connector->dy = -1.0;
        }
    } else {
        // The connector is oriented vertically, so the pin number depends
        // on the y position of the pointer.
        delta = y_root - pos[1];
        p = numPins*delta/height;
        if(p < 0.0) pinNum = 0;
        else if(p >= numPins) pinNum = numPins - 1;
        else pinNum = p;
        pin = c->pins + pinNum;
        pin->connector->x = pos[0] + width/2.0 - layoutPos[0];
        pin->connector->y = pos[1] + (pinNum + 0.5)*(height/numPins) -
                layoutPos[1];
        if(pin->connector->isSouthWestOfBlock) {
            pin->connector->dx = 1.0;
            pin->connector->dy = 0.0;
        } else {
            pin->connector->dx = -1.0;
            pin->connector->dy = 0.0;
        }
    }

    return pin;
}


// When fromPin is set we are dragging a connection line from that
// connector pin.

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

    if(fromPin && fromPin->connector == c)
        // We have a selected pin in this connector and we are still
        // there.  We leave the popover alone.  Changing the popover text
        // now would be miss-leading.
        return;


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
        // The connector is oriented horizontally; so the pin number
        // depends on the x position of the pointer.
        delta = e->x_root - pos[0];
        p = numPins*delta/width;
        if(p < 0.0) pinNum = 0;
        else if(p >= numPins) pinNum = numPins - 1;
        else pinNum = p;
        x = pos[0] + pinNum*(width/numPins) - layoutPos[0];
        y = (pos[1] + height) - layoutPos[1];
    } else {
        // The connector is oriented vertically, so the pin number depends
        // on the y position of the pointer.
        delta = e->y_root - pos[1];
        p = numPins*delta/height;
        if(p < 0.0) pinNum = 0;
        else if(p >= numPins) pinNum = numPins - 1;
        else pinNum = p;
        x = pos[0] + width - layoutPos[0];
        y = pos[1] + pinNum*(height/numPins) - layoutPos[1] +
                (height/numPins - MIN_POPOVER_HEIGHT)/2.0;
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

        DASSERT(c->pins[pinNum].desc);
        gtk_label_set_text(GTK_LABEL(connectorsPopover->label),
                c->pins[pinNum].desc);

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



gboolean ConnectorMotion_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {
WARN();
    if(!connectorsPopover && !(fromPin && fromPin->connector == c)) {
        // GTK+3 does not keep the order of events in a sensible way.
        // We sometimes get 2 enter events with no leave event.
        connectorsPopover = &c->block->page->connectorsPopover;
        gtk_widget_show(connectorsPopover->container);
    }

    ShowPinBalloon(draw, e, c);

    return FALSE; // False = do not eat event the layout may need it next.
}


gboolean ConnectorEnter_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {

    DASSERT(c);
    DASSERT(c->numPins);
    DASSERT(draw == c->widget);
WARN();

    if(movingBlock) {
        if(popoverShowing) {
            gtk_widget_hide(connectorsPopover->container);
            popoverShowing = false;
        }
        return FALSE; // TRUE = eat event
    }

    if(!connectorsPopover && !(fromPin && fromPin->connector == c)) {
        // GTK+3 does not keep the order of events in a sensible way.
        // We sometimes get 2 enter events with no leave event.
        connectorsPopover = &c->block->page->connectorsPopover;
        gtk_widget_show(connectorsPopover->container);
        ShowPinBalloon(draw, e, c);
    }

    if(fromPin)
        // We are dragging a connection.
        if(c->block != fromPin->connector->block) {




        }


    return FALSE; // TRUE = eat event
}


gboolean ConnectorLeave_CB(GtkWidget *draw,
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


gboolean ConnectorRelease_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {

WARN("                     %s", c->block->block->name);

    if(fromPin) {
        StopDragingConnection(c->block->page);
    }

    DASSERT(draw == c->widget);
    return FALSE; // TRUE = eat event
}


// The connector is a GTK drawingArea widget, "draw".
gboolean ConnectorPress_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {

ERROR();
    DASSERT(c);
    DASSERT(draw == c->widget);
    DASSERT(c->active);
    DASSERT(c->pins);
    DASSERT(c->numPins);
    ASSERT(c->block->block->isSuperBlock == 0, "Write this code");

    if(e->button == CONNECT_BUTTON) {
        fromPin = GetConnectorPinAndPosition(draw,
                e->x_root, e->y_root, c);

#if 0
        // This is why GTK3+ sucks:  None of this will release the button
        // press grab.  When you press a mouse button on a widget that
        // widget with get the next release event, even when the mouse
        // pointer is not on that widget at the time of the release
        // event.
        XUngrabPointer(gdk_x11_display_get_xdisplay(
                    gdk_display_get_default()), CurrentTime);
        gdk_x11_ungrab_server();
        gtk_grab_remove(draw);
        gtk_grab_remove(c->block->ebox);
        gdk_x11_display_ungrab(gdk_display_get_default());
#endif

        // KLUDGE:
        //
        // Hiding the widget will release the "button press grab".
        gtk_widget_hide(draw);
        // We just used hide to release the "button press grab", so we
        // need to show now.  It was not a big widget anyway, so it's not
        // a big deal to hide and then show it immediately after.  It's
        // not likely that it will blink visibly.
        gtk_widget_show(draw);


        StartDragingConnection(c->block->page);

        // Event goes to next parent widget so we can draw on the layout
        // as the mouse pointer moves.

        return FALSE; // FALSE = pass the event to the parent block widget.
    }


    return FALSE; // TRUE = eat this event
}
