#include <math.h>
#include <gtk/gtk.h>

#include "../include/quickstream.h"

#include "../lib/debug.h"

#include "quickstreamGUI.h"



#define MIN_PIN_WIDTH  (4.0) // In pixels
#define PIN_HEIGHT     

// Configuring background colors for the different connection terminal
// types:
//
#define ALPHA  (0.3)


struct Port *fromPort = 0;

double fromX = -1.0, fromY = -1.0;


static const struct Color cBGColor[NumTypes] = {
    {
        .r = 0.7, .g = 1.0, .b = 0.0, .a = ALPHA
    }, {
        .r = 0.95, .g = 0.8, .b = 0.0, .a = ALPHA
    }, {
        .r = 0.0, .g = 1.0, .b = 1.0, .a = ALPHA
    }, {
        .r = 0.3, .g = 0.8, .b = 0.9, .a = ALPHA
    }
};


static const struct Color
    // The regular connector terminal pin color
    pinFill = { .r = 0.0625, .g = 0.392, .b = 0.549, .a = 0.99 },
    pinLine = { .r = 0.0, .g = 0.0, .b = 0.0, .a = 0.99 },
    // The required (req) connector terminal pin color
    reqFill = { .r = 0.6, .g = 0.0, .b = 0.0, .a = 0.8 },
    reqLine = { .r = 0.2, .g = 0.1, .b = 0.1, .a = 0.8 },
    // Color for added mark for pins with graph port alias
    aLine = { .r = 0.99, .g = 0.6, .b = 0.9, .a = 1.0 };



// Each port/pin looks something like so:
//
//
//                      _____   _____
//  2 inputs like:      \   /   \   /
//           ____________|_|_____|_|___ looks like a golf tee
//          |
//          |
//
//                        _      _
//  2 outputs like:      / \    / \           .
//           ____________|_|____|_|___ looks like an arrow
//          |
//          |
//
//
// or that rotated 90, 180, or 270 degrees.
//

// The height of a pin will be this same height as the drawing area.
static const double pinHeight = (double) CON_THICKNESS;



static inline void ColorIt(cairo_t *cr, bool required) {

    if(required)
        cairo_set_source_rgba(cr,
                reqFill.r, reqFill.g, reqFill.b, reqFill.a);
    else
        cairo_set_source_rgba(cr,
                pinFill.r, pinFill.g, pinFill.b, pinFill.a);
    cairo_fill_preserve(cr);

    if(required)
        cairo_set_source_rgba(cr,
                reqLine.r, reqLine.g, reqLine.b, reqLine.a);
    else
        cairo_set_source_rgba(cr,
                pinLine.r, pinLine.g, pinLine.b, pinLine.a);
    cairo_stroke(cr);
}



// Draw an input terminal pin
//
// Draw Input Icon at 0,0 with golf tee up right and the origin at center
// and top.  Remember x increases to the right and y increases down.
// Note: We draw with a finite line width which we color too.
//
//
//     |<----w ---->|
// 1,7 _____________ 2 ___
//     \           /    ^
//      \         /     |
//      6\       /3     | 
//        |     |       h
//        |     |       |      
//       5|_____|4 _____V_
//                   
//
static inline void DrawInputIcon(cairo_t *cr, bool required) {

    const double w = PORT_ICON_WIDTH;
    const double h = pinHeight;

    // HW is Half the line Width

    cairo_move_to(cr, -w/2.0+HW, HW);    // 1
    cairo_line_to(cr, w/2.0-HW, HW);     // 2
    cairo_line_to(cr, w/5.0-HW, h/2.5);  // 3
    cairo_line_to(cr, w/5.0-HW, h-HW);   // 4
    cairo_line_to(cr, -w/5.0+HW, h-HW);  // 5
    cairo_line_to(cr, -w/5.0+HW, h/2.5); // 6
    cairo_close_path(cr);                // 7
    cairo_set_line_width(cr, PIN_LW);
}


// Draw an output terminal pin
//
// Draw Output Icon at 0,0 with arrow up right and the origin at center
// and top.  Note: We draw with a finite line width which we color too.
//
//
//      8  9,1
//       __  ___________           
//      /  \          ^
//     /    \         |
//    /      \        |
//  7/__6  3__\ 2     h
//     |    |         |
//     |    |         |      
//     |    |         |      
//    5|____|4 _______V_
//                   
//  |<---w --->|
//
static inline void DrawOutputIcon(cairo_t *cr, bool required) {

    const double w = PORT_ICON_WIDTH;
    const double h = pinHeight;

    // HW is Half the line Width

    cairo_move_to(cr, w/10.0-HW, HW);    // 1
    cairo_line_to(cr, w/2.0-HW, h/2.5);  // 2
    cairo_line_to(cr, w/5.0-HW, h/2.5);  // 3
    cairo_line_to(cr, w/5.0-HW, h-HW);   // 4
    cairo_line_to(cr, -w/5.0+HW, h-HW);  // 5
    cairo_line_to(cr, -w/5.0+HW, h/2.5); // 6
    cairo_line_to(cr, -w/2.0+HW, h/2.5); // 7
    cairo_line_to(cr, -w/10.0+HW, HW);   // 8
    cairo_close_path(cr);                // 9
    cairo_set_line_width(cr, PIN_LW);
}


// Draw a port connector thingy centered at i along the larger dimension
// of the drawing area.  And figure out how it's positioned/oriented.
//
static inline void DrawPort(cairo_t *cr, double i,
        bool isInputIcon, enum Pos pos, bool required,
        bool hasGraphAlias) {

    cairo_identity_matrix(cr);

    switch(pos) {

        case Right:
            cairo_translate(cr, pinHeight, i);
            cairo_rotate(cr, M_PI/2.0);
            break;
        case Left:
            cairo_translate(cr, 0, i);
            cairo_rotate(cr, -M_PI/2.0);
            break;
        case Top:
            cairo_translate(cr, i, 0);
            break;
        case Bottom:
            cairo_translate(cr, i, pinHeight);
            cairo_rotate(cr, M_PI);
            break;
        case NumPos:
            ASSERT(0);
    }


    // Now that we are in the right position and orientation draw
    // the port pin icon.

    if(isInputIcon)
        DrawInputIcon(cr, required);
    else
        DrawOutputIcon(cr, required);
 
    ColorIt(cr, required);

    if(hasGraphAlias) {
        // Draw a line over the connector pin.
        double LineWidth = PORT_ICON_WIDTH/9.0;
        cairo_move_to(cr, 0, 0); // 1
        cairo_line_to(cr, 0, pinHeight);  // 2
        cairo_set_line_width(cr, LineWidth);
        cairo_set_source_rgba(cr, aLine.r, aLine.g, aLine.b, aLine.a);
        cairo_stroke(cr);
    }
}


// Draw the drawing onto a surface that gets copied into the widget in
// Draw_cb().
//
// By Terminal we mean all the ports (or pins) that are of a given
// type.
static void DrawTerminal(const struct Terminal *t) {

    DASSERT(t);
    DASSERT(t->surface);

    // Paint to the surface, where we store our drawing pixel state
    cairo_t *cr = cairo_create(t->surface);
    DASSERT(cr);

    // First set the background color.
    const struct Color *bc = cBGColor + t->type;
    cairo_set_source_rgba(cr, bc->r, bc->g, bc->b, bc->a);
    cairo_paint(cr);

    uint32_t num = t->numPorts;

    double delta = t->width/(num+1);

    switch(t->type) {

        case In:
            for(uint32_t i=0; i<num; ++i)
                DrawPort(cr, delta * (i + 1), true/*isInputIcon*/,
                        t->pos, t->ports[i].isRequired,
                        t->ports[i].graphPortAlias);
            break;
        case Out:
            for(uint32_t i=0; i<num; ++i)
                DrawPort(cr, delta * (i + 1), false/*isInputIcon*/,
                        t->pos, t->ports[i].isRequired,
                        t->ports[i].graphPortAlias);
            break;
        case Set:
            for(uint32_t i=0; i<num; ++i)
                DrawPort(cr, delta * (i + 1), true/*isInputIcon*/,
                        t->pos, false/*isRequired*/,
                        t->ports[i].graphPortAlias);
            break;
        case Get:
            for(uint32_t i=0; i<num; ++i)
                DrawPort(cr, delta * (i + 1), false/*isInputIcon*/,
                        t->pos, false/*isRequired*/,
                        t->ports[i].graphPortAlias);
            break;
        case NumTypes:
            ASSERT(0);
    }

    cairo_destroy(cr);

    gtk_widget_queue_draw(t->drawingArea);
}


// Position x is in drawing area widget coordinate space
// along the longer dimension.
//
// Returns -1 if this x is not at or near a port (pin).
static inline
uint32_t PortPositionToIndex(const struct Terminal *t, double x) {

    x = x * (t->numPorts + 1)/t->width - 1;

// FAC is 1/2 the faction of the space for the port area that is active,
// so 0.4 means that we use 0.8 times the size of the port area as active
// for the popover showing that port text descriptor; leaving 0.2 as the
// faction of dead space to either side.
//
#define FAC  (0.4) // FAC is greater than 0.05 and less than 0.5

    if(x - FAC >= (double) (t->numPorts - 1) ||
            x + FAC <= 0.0)
        // It's past the last port or before the first port.
        return -1;

    double roundX = floorl(x + 0.5);

    if(x > roundX + FAC || x < roundX - FAC)
        return -1;

    return (uint32_t) roundX + 0.1;
}


static inline struct Port
*GetTerminalPort(double x, double y,
        struct Terminal *t) {

    if(t->numPorts == 0)
        // Got nothing.
        return 0;

    double d;

    switch(t->pos) {
        case Left:
        case Right:
            d = y;
            break;
        case Top:
        case Bottom:
            d = x;
            break;
        case NumPos:
            ASSERT(0);
    }

    uint32_t portNum = PortPositionToIndex(t, d);
    if(portNum == -1)
        // Got nothing;
        return 0;

    return t->ports + portNum;
}


static inline void ClearSurface(cairo_surface_t *s) {

    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
}


// This puts the drawing from the surface onto the GTK widget.
static gboolean
Draw_cb(GtkWidget *da, cairo_t *cr, struct Terminal *t) {

    DASSERT(t);
    DASSERT(t->surface);

    if(t->needSurfaceRedraw) {
        ClearSurface(t->surface);
        // Draw the terminal again because something has changed.
        DrawTerminal(t);
        t->needSurfaceRedraw = false;
    }

    cairo_set_source_surface(cr, t->surface, 0, 0);
    cairo_paint(cr);

    return TRUE;
}


// We found a GTK3 bug where Configure_cb() gets called
// with the drawing area allocation being (-1,-1,1,1); so we just work
// around it and wait of the "real" allocation that has numbers like
// GtkAllocation a=(103, 2, 21, 103).

// Called if the drawing area widget changed size or is new:
static gboolean
Configure_cb(GtkWidget *da, GdkEventConfigure *e, struct Terminal *t) {

    DASSERT(t);
    DASSERT(da);
    DASSERT(t->drawingArea == da);

    // Work around GTK3 bug: allocation being (-1,-1,1,1)
    GtkAllocation a;
    gtk_widget_get_allocation(da, &a);
    if(a.x == -1) return TRUE;

    t->width = (a.width > a.height)?a.width:a.height;

    if(t->surface)
        cairo_surface_destroy(t->surface);

    t->surface = gdk_window_create_similar_surface(
            gtk_widget_get_window(da),
            CAIRO_CONTENT_COLOR_ALPHA,
            gtk_widget_get_allocated_width(da),
            gtk_widget_get_allocated_height(da));

    DrawTerminal(t);

    return TRUE; // TRUE => no need for further processing.
}


static struct TextPopover {

    GtkWidget *popover;

    GtkWidget *label;

    // The drawing area that the popover is "relative to"
    // is in this Terminal struct
    struct Terminal *terminal;

    // The port to show information about in the popover.
    uint32_t portNum;

} textPopover = { 0 };


void CleanupGTK(void) {

#if 0 // TODO: Do we need something like this?

    if(textPopover.popover && !textPopover.terminal) {
        g_object_unref(textPopover.popover);
        gtk_widget_destroy(textPopover.popover);
        textPopover.popover = 0;
        textPopover.terminal = 0;
    }
#endif
}


static inline
void HideTextPopover(void) {

    if(textPopover.popover) {

        textPopover.terminal = 0;
        gtk_widget_hide(textPopover.popover);
    }
}


static inline void 
PositionTerminal(struct Terminal *t, enum Pos pos) {

    DASSERT(t);
    uint32_t numPorts = t->numPorts;
    GtkWidget *da = t->drawingArea;
    DASSERT(da);
    DASSERT(t->block);
    GtkWidget *grid = t->block->grid;
    DASSERT(grid);

    // length is the longer dimension of the connection terminals
    int length = CON_LEN;
    if(length < (PORT_ICON_WIDTH + PORT_ICON_PAD) * (numPorts+1))
        length = (PORT_ICON_WIDTH + PORT_ICON_PAD) * (numPorts+1);

    switch(pos) {
        case Left:
        case Right:
            gtk_widget_set_size_request(da, CON_THICKNESS, length);
            break;
        case Top:
        case Bottom:
            gtk_widget_set_size_request(da, length, CON_THICKNESS);
            break;
        case NumPos:
            ASSERT(0);
    }

    switch(pos) {
        case Left:
            gtk_grid_attach(GTK_GRID(grid), da, 0, 0, 1, 5);
            break;
        case Right:
            gtk_grid_attach(GTK_GRID(grid), da, 2, 0, 1, 5);
            break;
        case Top:
            gtk_grid_attach(GTK_GRID(grid), da, 1, 0, 1, 1);
            break;
        case Bottom:
            gtk_grid_attach(GTK_GRID(grid), da, 1, 4, 1, 1);
            break;
        case NumPos:
            ASSERT(0);
    }

    t->pos = pos;
}


void GetParameterPopoverText(const struct Port *p, char *text,
        size_t Len) {


    enum QsValueType vtype =  qsParameter_getValueType(
            (void *) p->qsPort);
    size_t size = qsParameter_getSize((void *) p->qsPort);
    uint32_t num;
    char *kind;
    switch(vtype) {
        case QsValueType_double:
            DASSERT(!(size % sizeof(double)));
            num = size/sizeof(double);
            kind = "double";
            break;
        case QsValueType_float:
            DASSERT(!(size % sizeof(float)));
            num = size/sizeof(float);
            kind = "float";
            break;
        case QsValueType_uint64:
            DASSERT(!(size % sizeof(uint64_t)));
            num = size/sizeof(uint64_t);
            kind = "uint64";
            break;
        case QsValueType_bool:
            DASSERT(!(size % sizeof(bool)));
            num = size/sizeof(bool);
            kind = "bool";
            break;
        case QsValueType_string32:
            DASSERT(!(size % 32));
            num = size/32;
            kind = "string32";
            break;

        default:
            ASSERT(0, "Write more QsValueType code here");
            break;
    }

    DASSERT(num);


    if(p->graphPortAlias)
        snprintf(text, Len, "%cetter \"%s\" type %s[%" PRIu32 "]"
                "\n  Graph Port Alias: \"%s\"",
                (p->terminal->type == Set)?'S':'G', p->name,
                kind, num,
                p->graphPortAlias);
    else
        snprintf(text, Len, "%cetter \"%s\" type %s[%" PRIu32 "]",
                (p->terminal->type == Set)?'S':'G', p->name,
                kind, num);
}


static inline void SetNewFromPort(struct Terminal *t) {

    // We are now button pressed on a port to connect from.

    GetPortConnectionPoint(&fromX, &fromY, fromPort);

    // Reset the newSurface that is currently drawn to nothing.
    // Otherwise the last line drawn will still be there if we
    // get a redraw in the layout.
    DrawLayoutNewSurface(t->block->layout, -1.0, -1.0, NumPos);

    // Cheap Trick, to drop the button press/release grab by this
    // drawingArea widget, by detaching and reattaching the da.
    //
    // We need to release the grab so that we can get motion events on
    // the "to Port" widget.
    //
    // Too keep the da from being destroyed when we detach it:
    GtkWidget *da = t->drawingArea;
    DASSERT(da);
    g_object_ref(da);
    gtk_container_remove(GTK_CONTAINER(t->block->grid), da);
    PositionTerminal(t, t->pos);
    // The parent of da now holds a reference, so we don't want this
    // reference from above anymore.
    g_object_unref(da);

    // Clear the newSurface.
    DrawLayoutNewSurface(t->block->layout, -1.0, -1.0, NumPos);
}


static gboolean PopoverLeave_cb(GtkWidget *w,
        GdkEventCrossing *e, void *data) {

    HideTextPopover();
    return FALSE;
}


// This checks if we need the popover first and then shows it if it needs
// to.  Do not call this if there is a button pressed.
static inline
void ShowTextPopover(struct Terminal *t, GdkEvent *e) {

    DASSERT(t);
    DASSERT(t->drawingArea);
    DASSERT(t->block);
    DASSERT(t->block->qsBlock);
    DASSERT(e);

    DASSERT(e->type == GDK_ENTER_NOTIFY || e->type == GDK_MOTION_NOTIFY);

    if(!t->ports) {
        DASSERT(t->numPorts == 0);
        HideTextPopover();
        return;
    }

    gdouble x;
    bool isX = (t->pos == Top) || (t->pos == Bottom);

    if(e->type == GDK_ENTER_NOTIFY) {
        GdkEventCrossing *ec = (GdkEventCrossing *) e;
        if(isX)
            x = ec->x;
        else
            x = ec->y;
    } else {
        GdkEventMotion *em = (GdkEventMotion *) e;
        if(isX)
            x = em->x;
        else
            x = em->y;
    }

    uint32_t portNum = PortPositionToIndex(t, x);

    if(portNum == -1) {
        HideTextPopover();
        return;
    }

    // Show it with port number portNum:

    if(textPopover.terminal == t && textPopover.portNum == portNum) {
        DASSERT(textPopover.popover);
        gtk_widget_show(textPopover.popover);
        // Nothing to change.
        return;
    }

    if(!textPopover.popover) {
        DASSERT(!textPopover.label);
        textPopover.popover = gtk_popover_new(t->drawingArea);

        // We need an extra reference to this widget so we can move it
        // between tabs and not have it destroyed when a tab is destroyed.
        //
        // Tests show that without this g_object_ref() call we get:
        // Gtk-CRITICAL **: 10:58:51.023: gtk_widget_hide: assertion
        // 'GTK_IS_WIDGET (widget)' failed  (and many more error messages)
        // When we use the popover in a tab/graph and then delete the
        // tab/graph and then use a different tabs with the popover.
        g_object_ref(textPopover.popover);
        // This widget may be realized so we must call
        // gtk_widget_add_events() and not gtk_widget_set_events().
        // What-ever the fuck that means...
        //gtk_widget_set_sensitive(textPopover.popover, FALSE);
        gtk_widget_add_events(textPopover.popover,
                GDK_LEAVE_NOTIFY_MASK);
        g_signal_connect(textPopover.popover, "leave-notify-event",
                G_CALLBACK(PopoverLeave_cb), 0);

        gtk_widget_set_name(textPopover.popover, "port_info");
        gtk_popover_set_modal(GTK_POPOVER(textPopover.popover), FALSE);
        textPopover.label = gtk_label_new("");
        gtk_container_add(GTK_CONTAINER(textPopover.popover),
                textPopover.label);
        gtk_widget_show(textPopover.label);
    }
    DASSERT(textPopover.label);

    textPopover.portNum = portNum;

    if(textPopover.terminal != t) {
        textPopover.terminal = t;
//DSPEW();
        gtk_popover_set_relative_to(GTK_POPOVER(textPopover.popover),
                        t->drawingArea);
//DSPEW();
    }

    GdkRectangle rec;

    int xi = PortIndexToPosition(t, portNum) + 0.5;
    // xi is center of the pin connector at port portNum.

    switch(t->pos) {

        case Top:
        case Bottom:
            rec.x = xi;
            rec.y = 0;
            break;
        case Left:
            rec.y = xi - CON_THICKNESS/4;
            rec.x = CON_THICKNESS/10;
            break;
        case Right:
            rec.y = xi - CON_THICKNESS/4;
            rec.x = 9*CON_THICKNESS/10;
            break;
        case NumPos:
            ASSERT(0);
    }
    // TODO: I do not know why a popover needs a width and height.
    rec.width = 0;
    rec.height = 0;

    gtk_popover_set_pointing_to(GTK_POPOVER(textPopover.popover),
                        &rec);

    DASSERT(textPopover.label);


    const size_t Len = 712;
    char text[Len];

    struct Port *p = t->ports + portNum;

    switch(t->type) {

        case In:
        case Out:
            if(p->graphPortAlias)
                snprintf(text, Len, "%sput stream port: %s"
                        "\n  Graph Port Alias: \"%s\"",
                        (t->type == In)?"In":"Out", p->name,
                        p->graphPortAlias);
            else
                snprintf(text, Len, "%sput stream port: %s",
                        (t->type == In)?"In":"Out", p->name);
            break;
        case Set:
        case Get:
            GetParameterPopoverText(p, text, Len);
            break;
        case NumTypes:
            ASSERT(0);
    }

    gtk_label_set_text(GTK_LABEL(textPopover.label), text);
    gtk_widget_show(textPopover.popover);

    return;
}


static gboolean
Enter_cb(GtkWidget *da, GdkEventCrossing *e, struct Terminal *t) {

    DASSERT(t);

    if(e->type != GDK_ENTER_NOTIFY)
        return FALSE;


    if(e->mode != GDK_CROSSING_NORMAL)
        return TRUE;


    if(e->detail == GDK_NOTIFY_ANCESTOR &&
            e->mode == GDK_CROSSING_NORMAL) {
        //ShowTextPopover(t, (GdkEvent *) e);
    }


    // Tests show we can get an enter event without getting a motion
    // event; so we have to treat the enter event like a motion event
    // too.


    return TRUE;
}


static gboolean
Leave_cb(GtkWidget *da, GdkEventCrossing *e, struct Terminal *t) {

    DASSERT(t);

    if(e->type != GDK_LEAVE_NOTIFY)
        return FALSE;

    // We do not need to hide the popover if the pointer entered the
    // popover.   We'll hide it when the pointer leaves the popover and
    // show it again if we enter the Terminal again.
    //
    if(e->detail == GDK_NOTIFY_ANCESTOR)
        HideTextPopover();

    return TRUE;
}


static gboolean
Motion_cb(GtkWidget *da, GdkEventMotion  *e, struct Terminal *t) {

    DASSERT(t);

    if(e->type != GDK_MOTION_NOTIFY)
        // WTF
        return TRUE;

    if(!movingBlocks)
        ShowTextPopover(t, (GdkEvent *) e);

    if(fromPort) {

        // We are drawing a connection line on the layout.
        DASSERT(t->block);
        DASSERT(t->block->layout);

        // Find the size of the block widget the pointer is in.
        GtkAllocation a;
        DASSERT(t->block);
        DASSERT(t->block->grid);
        gtk_widget_get_allocation(t->block->grid, &a);

        // We must get the pointer x,y position that is in the layout
        // coordinates so that we can draw on the layout surfaces.
        double x, y;

        // There are 2 cases times 4.
        //
        // First: Are we over a port that we can connect to?

        struct Port *toPort = GetTerminalPort(e->x, e->y, t);
        enum Pos toPos;

        if(toPort && CanConnect(fromPort, toPort)) {
            // Yes, we are over a port that we can connect to:

            // Find the x,y of the toPort and not the event.
            //
            // The toPort point is close but not at the event pointer
            // position.
 
            toPos = toPort->terminal->pos;

            // Note: we put the connection point under the block border,
            // at the outer edge of the drawing area of the terminal; and
            // centered on the port pin.

            // This checks the 4 position cases:
            GetPortConnectionPoint(&x, &y, toPort);

        } else if(toPort == fromPort) {

            x = -3.0;

        } else {

            if(toPort)
                toPort = 0;

            // Else: we are not over a port that we can connect to so we
            // keep dragging a straight line to the mouse pointer.

            toPos = NumPos;

            switch(t->pos) {
            case Left:
                x = t->block->x + BW + e->x;
                y = t->block->y + BW + e->y;
                break;
            case Right:
                x = t->block->x + a.width + e->x - CON_THICKNESS - BW;
                y = t->block->y + BW + e->y;
                break;
            case Top:
                x = t->block->x + CON_THICKNESS + BW + e->x;
                y = t->block->y + BW + e->y;
                break;
            case Bottom:
                x = t->block->x + BW + CON_THICKNESS + e->x;
                y = t->block->y + a.height + e->y - CON_THICKNESS - BW;
                break;
            default:
                ASSERT(0);
            }
        }

        // This will draw the new connection line or stop the drawing.
        HandleConnectingDragEvent(e, t->block->layout, x, y, toPos);
        //DSPEW("toPos=%d", toPos);
        return TRUE;
    }

    return FALSE; // TRUE => eat it here.
}


static gboolean
Press_cb(GtkWidget *da, GdkEventButton *e, struct Terminal *t) {

    DASSERT(t);

    if(NoGoodButtonPressEvent(e))
        // We don't use this event.
        return FALSE;

    if(textPopover.terminal)
        HideTextPopover();

    if(e->button == 1/*left*/) {

        DASSERT(t->block);
        DASSERT(t->block->layout);
        DASSERT(t->block->layout->window);
        DASSERT(t->block->layout->window->layout);
        DASSERT(t->block->layout->window->layout == t->block->layout);

        DASSERT(fromPort == 0);

        // This should be the only place where fromPort gets set to
        // non-zero.
        fromPort = GetTerminalPort(e->x, e->y, t);

        if(fromPort) {
            DASSERT(fromPort->qsPort);
            DASSERT(fromPort->terminal);
            if(fromPort->terminal->type == In && fromPort->graphPortAlias)
                // We cannot let the user connect from a stream input port
                // that has a graph port alias; otherwise the graph port alias
                // is useless.  You cannot connect to a stream input port more
                // than once.
                fromPort = 0;
        }


        if(!fromPort)
            // We are not over a connectable port, so let the under widget
            // do something with this event, if it wants to.
            return FALSE;

        SetNewFromPort(t);

        // We eat the event and we have dropped the grab too.
        return TRUE; // TRUE => eat the event
    }

    if(e->button == 3/*right*/) {
        ShowTerminalPopupMenu(t, GetTerminalPort(e->x, e->y, t));
        return TRUE;
    }

    return FALSE;
}


static gboolean
Release_cb(GtkWidget *da, GdkEventButton *e, struct Terminal *t) {

    DASSERT(t);

    if(e->type != GDK_BUTTON_RELEASE)
        return FALSE;

    if(e->button == 1/*left mouse*/ && fromPort) {

        // Clear the newSurface.
        DrawLayoutNewSurface(fromPort->terminal->block->layout,
                -1.0, -1.0, NumPos);

        struct Port *toPort = GetTerminalPort(e->x, e->y, t);

        if(toPort && CanConnect(fromPort, toPort)) {
            AddConnection(fromPort, toPort, true/*qsConnect?*/);
            toPort = 0;
        }

        fromPort = 0;

        return TRUE;
    }


    return FALSE;
}


// pos is Left, Right, Top, Bottom, and NumPos is the invalid Pos
void MoveTerminal(struct Terminal *t, enum Pos pos) {

    DASSERT(t);
    DASSERT(t->drawingArea);
    DASSERT(t->block);
    DASSERT(t->block->grid);
    DASSERT(pos < NumPos);

    if(t->pos != pos) {

        // This is the case where we are switching two terminal
        // positions.
        //
        // The terminal we'll be switching with.
        struct Terminal *st = 0;
        // Find the terminal we are switching with, st.
        for(enum Type ty = 0; ty != NumTypes; ++ty) {
            struct Terminal *ot = t->block->terminals + ty;
            if(ot->pos == pos) {
                // We are switching with this terminal
                st = ot;
                break;
            }
        }

        DASSERT(st);
        DASSERT(st->drawingArea);
        DASSERT(st->block);
        DASSERT(st->block->grid);

        g_object_ref(st->drawingArea);
        gtk_container_remove(GTK_CONTAINER(st->block->grid),
                st->drawingArea);

        g_object_ref(t->drawingArea);
        gtk_container_remove(GTK_CONTAINER(t->block->grid),
                t->drawingArea);

        PositionTerminal(st, t->pos);
        PositionTerminal(t, pos);

        g_object_unref(st->drawingArea);
        g_object_unref(t->drawingArea);

    } else {
        // Just setting up the terminal for the first time.
        PositionTerminal(t, pos);
    }
}


void CreateTerminal(struct Terminal *t) {

    DASSERT(t);
    DASSERT(t->block);
    DASSERT(t->type < NumTypes);
    DASSERT(t->type >= 0);

    // Note the memory for the struct Terminal is already
    // allocated in the struct Block as a [4] array.
    DASSERT(t == t->block->terminals + t->type);


    GtkWidget *drawingArea = gtk_drawing_area_new();
    gtk_widget_set_events(drawingArea,
            gtk_widget_get_events(drawingArea)|
            GDK_BUTTON_PRESS_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_ENTER_NOTIFY_MASK | 
            GDK_LEAVE_NOTIFY_MASK | 
            GDK_POINTER_MOTION_MASK|
            GDK_STRUCTURE_MASK);

    g_signal_connect(drawingArea,"configure-event",
            G_CALLBACK(Configure_cb), t);
    g_signal_connect(drawingArea, "draw",
            G_CALLBACK(Draw_cb), t);
    g_signal_connect(drawingArea, "enter-notify-event",
            G_CALLBACK(Enter_cb), t);
    g_signal_connect(drawingArea, "leave-notify-event",
            G_CALLBACK(Leave_cb), t);
    g_signal_connect(drawingArea, "motion-notify-event",
            G_CALLBACK(Motion_cb), t);
    g_signal_connect(drawingArea, "button-press-event",
            G_CALLBACK(Press_cb), t);
    g_signal_connect(drawingArea, "button-release-event",
            G_CALLBACK(Release_cb), t);

    t->drawingArea = drawingArea;

    // Create/allocate the struct Port array of ports
    // for this new terminal.
    CreatePorts(t);

    MoveTerminal(t, t->pos);
}
