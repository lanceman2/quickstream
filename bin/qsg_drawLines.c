#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "../lib/Dictionary.h"
#include "../include/quickstream.h"

#include "quickstreamGUI.h"



const struct Color
    // layout background (bg) color
    bg = { .r = 0.8, .g = 0.8, .b = 0.8, .a = 1.0 };

static const struct Color
    sel = { .r = 0.1, .g = 0.8, .b = 0.8, .a = 0.36 };

// Draw a "nice curve" connecting the 2 ports that are at x0, y0 and x3,
// y3.   This figures out the cubic Bézier spline control points x2, y2,
// x3, and y3.
//
// https://cairographics.org/manual/cairo-Paths.html#cairo-curve-to
//
static inline void DrawPortToPortCurve(cairo_t *cr,
        double x0, double y0, double x3, double y3, 
        enum Pos pos0, enum Pos pos3) {

    DASSERT(pos0 != NumPos);
    DASSERT(pos3 != NumPos);


    // We'll change x1,y1 and x2,y2 a little more below.
    double x1 = x0, y1 = y0, x2 = x3, y2 = y3;

    // TODO: we could spend lots of time figuring out good delta values
    // based on the distance between the 2 ports and the orientation of
    // the 2 port terminals.  Currently the case where a connection goes
    // directly back through a block with both ports points opposite each
    // other sucks.  All other cases seem okay.
    //
    // For example we need to make it smaller then the two terminals face
    // each other and are close to each other, otherwise the curve will
    // make loops in it; it should be small for that case.

    // Here we are toning the Bézier control points.
    double max = 100.0;

    // The distance between the ends.
    double delta = 0.3*sqrt((x0 - x3)*(x0 - x3) + (y0 - y3)*(y0 - y3));

    if(delta > max)
        delta = max;


    switch(pos0) {

        // The line should pointing in the correct direction out of the
        // block from the connection end point.
        //
        // Offset the 1st Bézier control point from the start point
        // based on the way the terminal faces.
        case Left:
            x1 -= delta;
            break;
        case Right:
            x1 += delta;
            break;
        case Top:
            y1 -= delta;
            break;
        case Bottom:
            y1 += delta;
            break;
        case NumPos:
            ASSERT(0);
    }

    // We reuse the use of the "delta" variable, it's value is
    // symmetrically dependent on the points x0,y0 and x3, y3.

    switch(pos3) {

        // Offset the 2nd Bézier control point from the end point based on
        // the way the terminal faces; like the above switch.
        case Left:
            x2 -= delta;
            break;
        case Right:
            x2 += delta;
            break;
        case Top:
            y2 -= delta;
            break;
        case Bottom:
            y2 += delta;
            break;
        case NumPos:
            ASSERT(0);
    }

    // Starting point.
    cairo_move_to(cr, x0, y0);

    // Draw the curve from the starting point set above in
    // cairo_move_to().
    cairo_curve_to(cr, x1, y1, x2, y2, x3, y3);
}


static inline void SetLineColorAndWidth(cairo_t *cr, enum Type type,
        const struct Port *p/*one of the ports in the connection*/) {

    // This is the only place we set the connection line width:
    cairo_set_line_width(cr, 3.2);

    // This is the only place we set these line colors.
    //
    // SET CONNECTION LINE COLORS HERE
    //
    switch(type) {
        // 4 Connector types, but just 2 colors:
        case In:
        case Out:
            // In or Out connect to each other
            cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.7);
            return;
        case Set:
        case Get:
            DASSERT(p);
            DASSERT(p->terminal);
            if(qsParameter_isGetterGroup((void *) p->qsPort)||
                    p->terminal->type == Get)
                cairo_set_source_rgba(cr, 1.0, 0.37, 1.0, 0.7);
            else
                cairo_set_source_rgba(cr, 0.25, 0.37, 1.0, 0.7);
            return;
        default:
            ASSERT(0);
    }
}


// Draw a single connection line.
void DrawConnectionLineToSurface(cairo_t *cr,
        const struct Port *p1,
        const struct Port *p2) {

    DASSERT(p1);
    DASSERT(p1->terminal);
    DASSERT(p2);
    DASSERT(p2->terminal);
 
    // We can use either p1 or p2 to get the color and line
    // width.  The result should be the same, given that the
    // connection rules are obeyed.
    SetLineColorAndWidth(cr, p1->terminal->type, p1);

    // Get the x,y positions of the line or curve end points:
    double x0, y0, x3, y3;
    GetPortConnectionPoint(&x0, &y0, p1);
    GetPortConnectionPoint(&x3, &y3, p2);
    // draw curves.
    DrawPortToPortCurve(cr, x0, y0, x3, y3,
            p1->terminal->pos, p2->terminal->pos);
    cairo_stroke(cr);
}


// Draw the background and a selection box or a new connection line that
// is unfinished that are in the Graph in the other overlay buffer in
// l->newSurface.
void DrawLayoutNewSurface(struct Layout *l,
        double x, double y, enum Pos toPos) {

    DASSERT(l);
    DASSERT(l->window);
    DASSERT(l->window->layout == l);

    cairo_t *cr = cairo_create(l->newSurface);
    DASSERT(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);


    if(layoutSelection.active) {
        // Draw the selection box.
        cairo_set_source_rgba(cr, sel.r, sel.g, sel.b, sel.a);
        cairo_rectangle(cr, layoutSelection.x, layoutSelection.y,
                layoutSelection.width, layoutSelection.height);
        cairo_fill(cr);
        cairo_destroy(cr);
        return;
    }


    if(!fromPort || x < -0.1) {
        cairo_destroy(cr);
        return;
    }

    // Draw an unfinished connection line:
    //
    //cairo_identity_matrix(cr);
    SetLineColorAndWidth(cr, fromPort->terminal->type, fromPort);


    if(toPos != NumPos)
        // This is the case where we can draw lines like there will be
        // when they are connecting to 2 good ports; using a cubic Bézier
        // spline.
        //
        DrawPortToPortCurve(cr, fromX, fromY, x, y,
                fromPort->terminal->pos, toPos);
    else {
        // Case where we do not have a good Port to connect to, we just
        // draw a single straight line for GUI user feedback.
        cairo_move_to(cr, fromX, fromY);
        cairo_line_to(cr, x, y);
    }

    cairo_stroke(cr);
    cairo_destroy(cr);
}
