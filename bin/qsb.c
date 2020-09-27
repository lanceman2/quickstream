#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>

#include <gtk/gtk.h>
#include <gdk/gdkevents.h>

#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>

#include "../lib/debug.h"

#include "qsb.h"


#define BLOCK_CREATE_BUTTON   (1)
#define BLOCK_CONNECT_BUTTON  (1)
#define BLOCK_MOVE_BUTTON  (1)



static GtkTextView *status = 0;

// Changing cursor is not well supported in GTK+3 (as of version 3.24.20):
//   1. CSS setting of cursor does not work at all.
//   2. Setting the cursor with  gdk_window_set_cursor() for a widget
//      sets it for all it's children, so that makes it difficult to
//      manage cursors.  There appears to be no stack of cursors,
//      which is the obvious way to manage changing cursors in code.

// List of cursors with images:
// https://developer.gnome.org/gdk3/stable/gdk3-Cursors.html#GdkCursorType
static GdkCursor *moveCursor = 0;
static GdkCursor *getCursor = 0;
static GdkCursor *inputCursor = 0;


static GtkWidget *window = 0;

static bool waitingForConnectEnter = false;

const static double lineWidth = 4.2;


static void ClearNewConnectionDraw(struct Page *page) {

    page->from = 0;
    waitingForConnectEnter = false;
}


static inline void SetCursor(GtkWidget *w, GdkCursor *cursor) {
    gdk_window_set_cursor(gtk_widget_get_window(w), cursor);
}


void
WriteStatus(const char *fmt, ...) {
    const size_t len = 1024;
    int size = 0;
    char buf[len];
    va_list ap;
    va_start(ap, fmt);
    size = vsnprintf(buf, len, fmt, ap);
    va_end(ap);
    ASSERT(size > 0);

    NOTICE("STATUS append: %s", buf);
}


static inline void
ClearSurface(cairo_t *cr) {
  cairo_set_source_rgb(cr, 224/255.0, 233/255.0, 244/255.0);
  cairo_paint(cr);
}

// We have this stupid surface re-allocator because the GTK layout widget
// will not receive configure events, which are when the widget window
// resizes.
static inline cairo_surface_t
*GetSurface(struct Page *page, GtkWidget *widget, cairo_surface_t *s) {

    gint w = gtk_widget_get_allocated_width(widget);
    gint h = gtk_widget_get_allocated_height(widget);

    if(s == 0 || page->w != w || page->h != h) {

ERROR("           GOT RESIZE");

        if(s)
            cairo_surface_destroy(s);
        GdkWindow *win = gtk_widget_get_window(widget);

        s = gdk_window_create_similar_surface(win,
                CAIRO_CONTENT_COLOR_ALPHA, w, h);

        cairo_t *cr = cairo_create(s);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);

        page->w = w;
        page->h = h;

        return s;
    }
    return 0;
}



static inline void GetConnectionColor(struct Connector *c,
        double *r, double *g, double *b, double *a) {

    switch(c->type) {
        case CT_INPUT:
            *r = 1.0;
            *g = 0.0;
            *b = 0.0;
            *a = 0.5;
            break;
        case CT_OUTPUT:
            *r = 1.0;
            *g = 0.0;
            *b = 0.0;
            *a = 0.5;
            break;
        case CT_GET:
            *r = 0.0;
            *g = 0.0;
            *b = 1.0;
            *a = 0.5;
            break;
        case CT_SET:
            *r = 0.0;
            *g = 0.0;
            *b = 1.0;
            *a = 0.5;
    }
}


static bool CanConnect(struct Connector *from,
            struct Connector *to) {

    switch(from->type) {
        case CT_INPUT:
            if(to->type == CT_OUTPUT)
                return true;
            return false;
        case CT_OUTPUT:
            if(to->type == CT_INPUT)
                return true;
            return false;
        case CT_GET:
            if(to->type == CT_SET)
                return true;
            return false;
        case CT_SET:
            if(to->type == CT_GET)
                return true;
            return false;
#ifdef DEBUG
        default:
            DASSERT(0, "missing case");
            return false;
#endif
    }
}



/*               Block

 ***************************************
 *                set                  *
 *                                     *
 * input                        output *
 *                                     *
 *                get                  *
 ***************************************
*/


// Wouldn't is be nice if C had pass by reference.
//
// Figures out where to draw to or from for a connection
// to the input, output, get or set side of a block.
// All done relative to the layout widget.
//
static inline void GetConnectionPoint(const struct Connector *c,
        double *x, double *y) {

    DASSERT(c);
    struct Block *block = c->block;
    DASSERT(block);
    DASSERT(block->container);

    // TODO: deal with rotated blocks.

    // block width and height
    gint w = gtk_widget_get_allocated_width(block->container);
    gint h = gtk_widget_get_allocated_height(block->container);

    // Draw from (relative to the layout widget):
    *x = c->block->x, *y = c->block->y;

    switch(c->type) {
        case CT_INPUT:
            *y += h/2;
            break;
        case CT_OUTPUT:
            *x += w;
            *y += h/2;
            break;
        case CT_GET:
            *x += w/2;
            *y += h;
            break;
        case CT_SET:
            *x += w/2;
    }
}


/* All the connector image sizes are 32x32 pixels.  We use the 32 as part
 * of the distance to the cubic Bézier spline control point, that we use
 * to draw the lines with using cairo_curve_to().
 */

#define CONNECT_LEN  ((double) 32 * 3.5)


// Gets 4 points that are used to draw connections using cubic Bézier
// spline using 2 points for the 2 connectors, giving 4 points for the
// cubic Bézier spline curve for each connection.
// 
// See:
// https://www.cairographics.org/manual/cairo-Paths.html#cairo-curve-to
// https://www.cairographics.org/samples/curve_to/
// https://stackoverflow.com/questions/15374806
//
static inline void GetConnectionPoints(const struct Connector *c,
        double *x0, double *y0, double *x1, double *y1) {

    DASSERT(c);
    struct Block *block = c->block;
    DASSERT(block);
    DASSERT(block->container);

    GetConnectionPoint(c, x0, y0);

    // TODO: deal with rotated blocks.

    switch(c->type) {
        case CT_INPUT:
            *x1 = *x0 - CONNECT_LEN;
            *y1 = *y0;
            break;
        case CT_OUTPUT:
            *x1 = *x0 + CONNECT_LEN;
            *y1 = *y0;
            break;
        case CT_GET:
            *x1 = *x0; 
            *y1 = *y0 + CONNECT_LEN;
            break;
        case CT_SET:
            *x1 = *x0;
            *y1 = *y0 - CONNECT_LEN;
    }
}



static void DrawConnection(struct Connector *c0, struct Connector *c1,
        cairo_surface_t *s) {

    double x0, y0, x1, y1, x2, y2, x3, y3;
    GetConnectionPoints(c0, &x0, &y0, &x1, &y1);
    GetConnectionPoints(c1, &x3, &y3, &x2, &y2);
    double r, g, b, a;
    GetConnectionColor(c0, &r, &g, &b, &a);

    // TODO: if this is called in a loop we may need to keep the cr
    // alive between lines.
    //
    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_set_line_width(cr, lineWidth);
    cairo_move_to(cr, x0, y0);
    cairo_curve_to(cr, x1, y1, x2, y2, x3, y3);
    cairo_stroke(cr);
    cairo_destroy(cr);
}


static void RedrawOldLines(struct Page *page) {

    // draw onto page->oldLines
    cairo_t *cr = cairo_create(page->oldLines);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    // clear the surface
    cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
    cairo_paint(cr);

    for(size_t i=0; i<page->numConnections; ++i)
        DrawConnection(page->connections[i].from,
                page->connections[i].to,
                page->oldLines);
}




// The add to the record of connections between the blocks.
//
static void AddConnection(struct Connector *c0, struct Connector *c1) {

    DASSERT(c0);
    DASSERT(c1);
    DASSERT(c0->block);
    DASSERT(c1->block);
    DASSERT(c0->block->page);
    DASSERT(c1->block->page);
    struct Page *page = c0->block->page;
    DASSERT(page == c1->block->page);
    
    size_t num = page->numConnections;

    page->connections = realloc(page->connections, 
            sizeof(*page->connections)*(num+1));
    ASSERT(page->connections);

    if(c0->type == CT_OUTPUT || c0->type == CT_GET) {
        page->connections[num].from = c0;
        page->connections[num].to = c1;
    } else {
        page->connections[num].from = c1;
        page->connections[num].to = c0;
    }
    ++page->numConnections;
}


// Draw onto the cairo surface page->newLine the line as the mouse is
// pressed and the pointer moves.  The line is from the connector widget
// that is referred to with page->from (x0,y0) and to the pointer position
// (x1,y1).
//
void DrawDragLine(struct Page *page) {

    // GTK (the piece of shit) seems to be setting errno all the time.
    errno = 0;

    DASSERT(page);
    DASSERT(page->newLine);
    DASSERT(page->from);
    DASSERT(page->from->block);

    double x0, y0, x1, y1;
    GetConnectionPoint(page->from, &x0, &y0);
    GtkAllocation alloc;
    gtk_widget_get_allocation(page->from->widget, &alloc);
    x1 = page->pointer_x + alloc.x;
    y1 = page->pointer_y + alloc.y;

    // draw onto page->newLine
    cairo_t *cr = cairo_create(page->newLine);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    // clear the surface
    cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
    cairo_paint(cr);

    if(waitingForConnectEnter)
        // We are waiting for the "enter-notify-event"
        // for the other connector widget.  If a good
        // widget gets that event it will a draw the
        // line and log the connection into the more
        // permanent record.
        return;

    // Else we draw a temporary line as the pointer moves
    // and the mouse is still pressed.
    //
    // Set the line color
    double r, g, b, a;
    GetConnectionColor(page->from, &r, &g, &b, &a);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_set_line_width(cr, lineWidth);
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x1, y1);
    cairo_stroke(cr);
    cairo_destroy(cr);
}

static bool blockMoved = false;


/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean drawLayout(GtkWidget *widget, cairo_t *cr,
        struct Page *page)
{
    DASSERT(page);
    DSPEW();

    bool needOldLineRedraw = false;

    cairo_surface_t *n = GetSurface(page, widget, page->newLine);

    if(n) {
        // we resized or created new; so now these surfaces have nothing
        // on them.
        page->newLine = n;
        page->oldLines = GetSurface(page, widget, 0);
        // Draw any old lines to the oldLines surface.
        needOldLineRedraw = true;
    }

    if(needOldLineRedraw || blockMoved) {
        RedrawOldLines(page);
        blockMoved = false;
    }

    // 1. Clear cr
    ClearSurface(cr);

    // 2. make new line that are still being drawn with the moving mouse
    // or are just being finished.
    if(page->from && !waitingForConnectEnter) {
        // !waitingForConnectEnter == We NOT are waiting for the
        // "enter-notify-event" for the other connector widget.  If a good
        // widget gets that event it will a draw the line and log the
        // connection into the more permanent record.
        DrawDragLine(page);
        cairo_set_source_surface(cr, page->newLine, 0, 0);
        cairo_paint(cr);
    }

    // 3. Add the connections that we know so far.
    cairo_set_source_surface(cr, page->oldLines, 0, 0);
    cairo_paint(cr);

    return FALSE; // FALSE == Call other callbacks
}


static void CSS() {

    GtkCssProvider *provider;

    provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(
            gdk_display_get_default_screen(gdk_display_get_default()),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GError *error = 0;

    gtk_css_provider_load_from_data(provider,
        /* The CSS stuff in GTK3 version 3.24.20 does not work as
         * documented.  All the example codes that I found did not work.
         * I expect this is a moving target, and this will brake in the
         * future.
         */
            // This CSS works for GTK3 version 3.24.20
            //
            // #block > #foo
            //    is for child of parent named "block" with name "foo"
            //
            // Clearly widget name is not a unique widget ID; it's more
            // like a CSS class name then a name.  The documentation is
            // miss-leading on this.  This is not consistent with regular
            // www3 CSS.  We can probably count on this braking in new
            // versions of GTK3.
            //
            // This took 3 days of trial and error.  Yes: GTK3 sucks;
            // maybe a little less than QT.
            //
            // TODO: It'd be nice to put this in gtk builder files,
            // and compiling it into the code like the .ui widgets.
            // There currently does not appear to be a way to do this.
            "#vpanel {\n"
                "}\n"
            "#block {\n"
                "background-color: rgba(83,138,250,0.5);\n"
                "border: 1px solid black;\n"
                "}\n"
            "#block > #get, #set, #input, #output {\n"
                "background-color: rgba(147,176,223,0.5);\n"
                "border: 1px solid rgb(118,162,247);\n"
                "color: black;\n"
                "font-size: 130%;\n"
                "}\n"
            "#block > #get:hover {\n"
                "background-color: rgba(157,186,243,0.8);\n"
                "border: 1px solid rgb(100,142,207);\n"
            "}\n"
             "#block > #label {\n"
                "color: black;\n"
                "font-size: 130%;\n"
                "}\n"
            "#block > #bar:hover {\n"
                "color: red;\n"
                "background-color: rgba(255,90,10,0.5);\n"
                "}\n"
            , -1, &error);

    g_object_unref (provider);
}

static void PopForward(GtkLayout *layout, struct Block *b) {

    // 1. get one more reference to the widget; otherwise
    //    the widget will be destroyed in the next call.
    g_object_ref(G_OBJECT(b->container
                ));
    // 2. remove the widget which removes one reference
    gtk_container_remove(GTK_CONTAINER(layout), b->container);
    // Now we have at least one reference.
    // 3. re-add the widget which will take ownership of the
    //    one reference.
    gtk_layout_put(layout, b->container, b->x, b->y);
}


static gboolean
BlockButtonCB(GtkWidget *widget, GdkEventButton *e, struct Block *b) {

    errno = 0;
    static bool gotPress = false;
    static gint xOffset = 0, yOffset = 0;


    switch(e->type) {
    
        case GDK_BUTTON_PRESS:
        {
            if(e->button != BLOCK_MOVE_BUTTON)
                break;

            SetCursor(GTK_WIDGET(b->layout), moveCursor);
            xOffset = e->x_root;
            yOffset = e->y_root;
            PopForward(b->layout, b);
            gotPress = true;
            gtk_grab_add(widget);
            break;
        }

        case GDK_MOTION_NOTIFY:

            // We get e->x and e->y that are relative to the widget where
            // this event first was received by a callback and it may not
            // be this Layout widget; so we must use the x_root and y_root
            // position values, which are positions relative to root
            // (whatever the hell that is).
            if(waitingForConnectEnter)
                ClearNewConnectionDraw(b->page);

            if(!gotPress) break;

            blockMoved = true;

 
            b->x += ((gint) e->x_root) - xOffset;
            b->y += ((gint) e->y_root) - yOffset;

            xOffset = e->x_root;
            yOffset = e->y_root;
            gtk_layout_move(b->layout, b->container, b->x, b->y);
            break;

        case GDK_BUTTON_RELEASE:
        {
            SetCursor(GTK_WIDGET(b->layout), 0);

            if(e->button != BLOCK_MOVE_BUTTON) break;

            blockMoved = true;

            gtk_grab_remove(widget);

            gotPress = false;

            break;
        }

        default:
            break;
    }

    return TRUE; // FALSE = go to next widget
}

#if 0
static gboolean
BlockEnterCB(GtkWidget *w, GdkEvent *e, struct Block *b) {

    errno = 0;

    //DSPEW();
    if(movingBlock) return FALSE;

    return TRUE; // TRUE = do not go to next widget
}

static gboolean
BlockLeaveCB(GtkWidget *w, GdkEvent *e, struct Block *b) {

    errno = 0;

    //DSPEW();
    if(movingBlock) return FALSE;

    return TRUE; // TRUE = do not go to next widget
}
#endif



static
void QueueConnectionsDraw(struct Block *b) {

    DASSERT(b);
    GtkWidget *layout = GTK_WIDGET(b->layout);
    DASSERT(layout);

    gint w = gtk_widget_get_allocated_width(layout);
    gint h = gtk_widget_get_allocated_height(layout);
    /* Now invalidate the affected region of the drawing area. */
    gtk_widget_queue_draw_area(layout, 0, 0, w, h);
}


static gboolean
ConnectPressCB(GtkWidget *w, GdkEventButton *e, struct Connector *c) {

    errno = 0;

    DSPEW("type=%s", gtk_widget_get_name(w));

    DASSERT(e->type == GDK_BUTTON_PRESS);
    DASSERT(c);
    DASSERT(c->block);
    waitingForConnectEnter = false;
    c->block->page->from = c;

    DASSERT(c->block->page);
    // page->pointer_x|y be the pointer position relative to the layout.
    c->block->page->pointer_x = e->x + c->block->x;
    c->block->page->pointer_y = e->y + c->block->y;

    c->block->page->from = c;

    QueueConnectionsDraw(c->block);

    return TRUE; // TRUE = do not go to next widget
}


static gboolean
ConnectMotionCB(GtkWidget *w, GdkEventButton *e, struct Connector *c) {


    if(waitingForConnectEnter) {
        ClearNewConnectionDraw(c->block->page);
        return TRUE;
    }

    if(c->block->page->from) {
        c->block->page->pointer_x = e->x + c->block->x;
        c->block->page->pointer_y = e->y + c->block->y;

        QueueConnectionsDraw(c->block);
    }

    return TRUE; // TRUE = do not go to next widget
}


static gboolean
ConnectReleaseCB(GtkWidget *w, GdkEventButton *e, struct Connector *c) {

fprintf(stderr, "FUCKING RELEASE\n");

    errno = 0;

    DASSERT(e->type == GDK_BUTTON_RELEASE);
    DASSERT(c);
    DASSERT(c->block);
    struct Page *page = c->block->page;
    DASSERT(page);
    DASSERT(page->newLine);

    // Clear the new line surface
    cairo_t *cr = cairo_create(page->newLine);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
    cairo_paint(cr);
    cairo_destroy(cr);
    QueueConnectionsDraw(c->block);

    if(!page->from) {
        return TRUE;
    }

    if(page->from)
        waitingForConnectEnter = true;

    return TRUE; // TRUE = do not go to next widget
}

static gboolean
ConnectEnterCB(GtkWidget *w, GdkEvent *e, struct Connector *to) {

    errno = 0;
    DASSERT(GDK_ENTER_NOTIFY == e->type);

    DASSERT(to);
    DASSERT(to->block);
    struct Page *page = to->block->page;
    DASSERT(page);

    if(!page->from || !waitingForConnectEnter ||
            !CanConnect(page->from, to))
        return TRUE;

    DASSERT(page->from->block);
    DASSERT(page->from->block->page == page);

    ERROR("                                   ADDING NEW LINE");

    // We got a connect release just before this.  We now setup
    // a connection.
    //
    // TODO: add more here to record the connection.
    DrawConnection(page->from, to, page->oldLines);
    AddConnection(page->from, to);

    // Reset for next connection
    page->from = 0;
    waitingForConnectEnter = false;
    QueueConnectionsDraw(to->block);

    return TRUE; // TRUE = do not go to next widget
}

#if 0
static gboolean
ConnectLeaveCB(GtkWidget *w, GdkEvent *e, struct Connector *c) {

    errno = 0;
    DASSERT(GDK_LEAVE_NOTIFY == e->type);

    DSPEW("                                LEAVE");

    return TRUE; // TRUE = do not go to next widget
}
#endif

static void MakeBlockConnector(GtkWidget *grid,
        const char *type,
        const char *imageFile,
        gint x, gint y, gint w, gint h,
        struct Block *block,
        struct Connector *c
        ) {

    GtkWidget *ebox = gtk_event_box_new();
    c->widget = ebox;

    gtk_widget_set_can_focus(ebox, TRUE);
    gtk_widget_set_events(ebox,
            GDK_STRUCTURE_MASK |
            GDK_SCROLL_MASK |
            GDK_POINTER_MOTION_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_BUTTON_PRESS_MASK|
            GDK_STRUCTURE_MASK);

    g_signal_connect(ebox, "button-press-event",
            G_CALLBACK(ConnectPressCB), c);
    g_signal_connect(ebox, "motion-notify-event",
            G_CALLBACK(ConnectMotionCB), c);
    g_signal_connect(ebox, "button-release-event",
            G_CALLBACK(ConnectReleaseCB), c);
    g_signal_connect(ebox, "enter-notify-event",
            G_CALLBACK(ConnectEnterCB), c);

    //g_signal_connect(ebox, "leave-notify-event",
            //G_CALLBACK(ConnectLeaveCB), c);

    gtk_widget_set_name(ebox, type);
    gtk_widget_show(ebox);
    gtk_grid_attach(GTK_GRID(grid), ebox, x, y, w, h);

    GtkWidget *img = gtk_image_new_from_file(imageFile);
    gtk_container_add(GTK_CONTAINER(ebox), img);
    gtk_widget_set_name(img, type);
    gtk_widget_show(img);
}

static void MakeBlockLabel(GtkWidget *grid,
        const char *text,
        gint x, gint y, gint w, gint h) {

    GtkWidget *l = gtk_label_new(text);
    gtk_widget_set_name(l, "label");
    gtk_widget_show(l);
    gtk_grid_attach(GTK_GRID(grid), l, x, y, w, h);
}

#if 0
static void DestroyBlock(struct Block *block) {

    DASSERT(block);
    DASSERT(block->container);
    gtk_widget_destroy(block->container);
}
#endif

static struct Block *CreateBlock(struct Page *page,
        GtkLayout *layout,
        const char *name,
        double x, double y) {

    DASSERT(page);

    struct Block *block = calloc(1, sizeof(*block));
    ASSERT(block, "calloc(1,%zu) failed", sizeof(*block));

    block->page = page;
    
    // Setup the 4 connector types.
    block->get.type = CT_GET;
    block->get.block = block;
    block->set.type = CT_SET;
    block->set.block = block;
    block->input.type = CT_INPUT;
    block->input.block = block;
    block->output.type = CT_OUTPUT;
    block->output.block = block;


    GtkWidget *grid = gtk_grid_new();
    GtkWidget *ebox = gtk_event_box_new();

    block->container = ebox;
    block->layout = layout;
    block->x = x;
    block->y = y;

    // As of GTK3 version 3.24.20; gtk widget name is more like a CSS
    // class name.  Name is not a unique ID.  It's more like a CSS
    // class.
    gtk_widget_set_name(grid, "block");
    gtk_widget_set_visible(grid, TRUE);

    // Sets the minimum size of grid.
    gtk_widget_set_size_request(grid, -1, -1);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), FALSE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), FALSE);

    gtk_widget_set_can_focus(ebox, TRUE);
    gtk_widget_set_events(ebox,
                GDK_BUTTON_PRESS_MASK|
                GDK_BUTTON_RELEASE_MASK|
                GDK_POINTER_MOTION_MASK|
                GDK_ENTER_NOTIFY_MASK|
                GDK_LEAVE_NOTIFY_MASK
                );

    g_signal_connect(ebox, "button-press-event",
        G_CALLBACK(BlockButtonCB), block);
    g_signal_connect(ebox, "button-release-event",
        G_CALLBACK(BlockButtonCB), block);
    g_signal_connect(ebox, "motion-notify-event",
        G_CALLBACK(BlockButtonCB), block);
#if 0
     g_signal_connect(ebox, "leave-notify-event",
        G_CALLBACK(BlockLeaveCB), block);
    g_signal_connect(ebox, "enter-notify-event",
        G_CALLBACK(BlockEnterCB), block);
#endif
    gtk_widget_set_name(ebox, "get");
    gtk_widget_show(ebox);
    gtk_container_add(GTK_CONTAINER(ebox), grid);
    gtk_layout_put(layout, ebox, x, y);


    // The Grid of a block
    //
    // 0,0  1,0  2,0  3,0  4,0  5,0  6,0
    // 
    // 0,1  1,1  2,1  3,1  4,1  5,1  6,1
    //
    // 0,2  1,2  2,2  3,2  4,2  5,2  6,2
    //
    // 0,3  1,3  2,3  3,3  4,3  5,3  6,3
    //
    // 0,4  1,4  2,4  3,4  4,4  5,4  6,4

    MakeBlockLabel(grid, name, 2, 1, 3, 1);
    MakeBlockLabel(grid, "block type 923rj", 2, 2, 3, 1);
    MakeBlockLabel(grid, "Boston", 2, 3, 3, 1);

    MakeBlockConnector(grid, "input", "input.png", 0, 0, 1, 5, block, &block->input);
    MakeBlockConnector(grid, "output", "output.png", 6, 0, 1, 5, block, &block->output);
    MakeBlockConnector(grid, "set", "set.png", 1, 0, 4, 1, block, &block->set);
    MakeBlockConnector(grid, "get", "get.png", 1, 4, 4, 1, block, &block->get);

    gtk_widget_show(grid);

    return block;
}


static gboolean WorkAreaCB(GtkLayout *layout,
        GdkEventButton *e, struct Page *page) {

    DASSERT(moveCursor);
    DASSERT(window);

    static gint xOffset = 0, yOffset = 0;
    static struct Block *movingBlock = 0;
    errno = 0;

    switch(e->type) {

        case GDK_BUTTON_PRESS:
        {
            if(e->button != BLOCK_CREATE_BUTTON) break;

            // Create a new block and then move it.
            char *name = strdup("block Name larger block Name ya");
            movingBlock = CreateBlock(page, layout, name, e->x, e->y);

            xOffset = e->x_root;
            yOffset = e->y_root;

            SetCursor(GTK_WIDGET(layout), moveCursor);
            WriteStatus("created block \"%s\"", name);
            free(name);
            break;
        }

        case GDK_MOTION_NOTIFY:


            if(waitingForConnectEnter)
                ClearNewConnectionDraw(page);

            if(!movingBlock) break;

            // We get e->x and e->y that are relative to the widget where
            // this event first was received by a callback and it may not
            // be this Layout widget; so we must use the x_root and y_root
            // position values, which are positions relative to root
            // (which I guess is relative to the root X11 window).
            //
            movingBlock->x += ((gint) e->x_root) - xOffset;
            movingBlock->y += ((gint) e->y_root) - yOffset;

            xOffset = e->x_root;
            yOffset = e->y_root;

            gtk_layout_move(layout, movingBlock->container,
                    movingBlock->x, movingBlock->y);
            break;

        case GDK_BUTTON_RELEASE:

            SetCursor(GTK_WIDGET(layout), 0);

            if(e->button != BLOCK_CREATE_BUTTON) break;

            movingBlock = 0;
            break;

        default:
            break;
    }

    return FALSE; // FALSE = go to next widget
}


static inline void
Connect(GtkBuilder *builder, const char *id, const char *action,
        void *callback, void *userData) {
    g_signal_connect(gtk_builder_get_object(builder, id),
            action, G_CALLBACK(callback), userData);
}

static inline GdkCursor *GetCursor(const char *type) {
    return gdk_cursor_new_from_name(gdk_display_get_default(), type);
}

static inline GdkCursor *GetCursor_(GdkCursorType type) {
    return gdk_cursor_new_for_display(gdk_display_get_default(), type);
}


// Just to get a tab name like "Untitled_3"
static int tabCreateCount = 0;


static gboolean TestPress(GtkLayout *layout,
        GdkEventButton *e, struct Page *page) {

    DSPEW();

    return FALSE;
}






static GtkNotebook *noteBook = 0;


static inline void MakeWorkArea(struct Page *page) {

    GtkBuilder *b = gtk_builder_new_from_resource(
            "/quickstreamBuilder/qsb_workArea_res.ui");

    Connect(b, "workArea", "draw", drawLayout, page);

    Connect(b, "workArea", "button-press-event", TestPress, page);

    Connect(b, "workArea", "button-release-event", WorkAreaCB, page);
    Connect(b, "workArea", "button-press-event", WorkAreaCB, page);
    Connect(b, "workArea", "motion-notify-event", WorkAreaCB, page);

    status = GTK_TEXT_VIEW(gtk_builder_get_object(b, "status"));

    // As of gtk+3 version 3.24.20 "configure-event" does not work for
    // a GtkLayout widget.  That may be because the  GtkLayout widget
    // changes size as we add child widgets to it.
    //Connect(b, "workArea", "configure-event", ConfigureLayout, 0);

    char tagName[64];
    if(tabCreateCount) {
        ++tabCreateCount;
        snprintf(tagName, 64, "Untitled_%d", tabCreateCount);
    } else {
        ++tabCreateCount;
        strcpy(tagName, "Untitled");
    }

    gint rt = gtk_notebook_append_page(noteBook,
            GTK_WIDGET(gtk_builder_get_object(b, "vpanel")),
            gtk_label_new(tagName));
    ASSERT(rt >= 0);

    // We are done with this builder
    g_object_unref(G_OBJECT(b));
}


// TODO: free(page);


static gboolean NewTab(GtkWidget *w, gpointer data) {

    struct Page *page = calloc(1, sizeof(*page));
    ASSERT(page, "calloc(1, %zu) failed", sizeof(*page));

    MakeWorkArea(page);
    return TRUE;
}


static inline void
setup_widget_connections(void) {
    

    // From the XML files: qsb_res.xml and qsb_res.ui, a gObject compiler,
    // named glib-compile-resources, builds qsb_res.c.  qsb_res.c is
    // compiled into an object and the object is linked with this file.
    // The call to gtk_builder_new_from_resource() calls some generated
    // functions from qsb_resources.c to get the builder struct that has
    // lots of widgets in it which we connect action callbacks to by id;
    // as in for example: <object class="GtkMenuItem" id="quitMenu">.
    //
    // Similarly for qsb_workArea_res.* which are used to
    // get the main work area draw widget in MakeWorkArea() above.

    GtkBuilder *b = gtk_builder_new_from_resource(
            "/quickstreamBuilder/qsb_res.ui");


    ///////////////////////////////////////////////////////////////////
    //    Here is where cursors are configured
    //////////////////////////////////////////////////////////////////

    moveCursor = GetCursor("grabbing");

    getCursor = GetCursor_(GDK_BOTTOM_SIDE);
    inputCursor = GetCursor_(GDK_RIGHT_SIDE);

    //////////////////////////////////////////////////////////////////

    window = GTK_WIDGET(gtk_builder_get_object(b, "window"));

    {
        GdkGeometry geo;

        geo.min_width = 300;
        geo.min_height = 300;

        ASSERT(window);
        gtk_window_set_geometry_hints(GTK_WINDOW(window), 0, &geo, GDK_HINT_MIN_SIZE);

        // TODO: add the window size setting to user preferences
        // just before exiting the app.
        gtk_window_resize(GTK_WINDOW(window), 1000, 1000);
    }

    noteBook = GTK_NOTEBOOK(gtk_builder_get_object(b, "notebook"));

    NewTab(0, 0);

    Connect(b, "window", "destroy", gtk_main_quit, 0);
    Connect(b, "quitMenu", "activate", gtk_main_quit, 0);
    Connect(b, "quitButton", "clicked", gtk_main_quit, 0);
    Connect(b, "newTabButton", "clicked", NewTab, 0);

    // We are done with this builder
    g_object_unref(G_OBJECT(b));
}


void
catcher(int sig) {

    fprintf(stderr, "Caught signal %d\n"
        "\n"
        "  Consider running:  gdb -pid %u\n\n",
        sig, getpid());

    ASSERT(0);
}


int main(int argc, char *argv[]) {

    signal(SIGSEGV, catcher);
    signal(SIGABRT, catcher);

    gtk_init(&argc, &argv);

    CSS();

    setup_widget_connections();

    gtk_main();

    DSPEW("xmlReadFile=%p", xmlReadFile);
    return 0;
}
