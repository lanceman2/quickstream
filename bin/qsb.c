// How to poll() with GTK events:
//
//   g_main_context_set_poll_func()
//
// file:///usr/share/gtk-doc/html/glib/glib-The-Main-Event-Loop.html#g-main-context-set-poll-func
//
// This function could possibly be used to integrate the GLib event loop
// with an external event loop.


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

#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>

#include "../lib/debug.h"

#include "qsb.h"


#define BLOCK_CREATE_BUTTON   (3) // In layout
#define BLOCK_POPUP_BUTTOM    (3)
#define SELECT_BUTTON         (1) // In layout
#define BLOCK_CONNECT_BUTTON  (1)
#define BLOCK_MOVE_BUTTON     (1)



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


static void UnselectBlock(struct Block *b) {

    DASSERT(b);
    // Put it on the page selected stack.
    struct Page *page = b->page;
    DASSERT(page);
    g_tree_remove(page->selectedBlocks, b);
    gtk_widget_set_name(b->grid, "block");
}

static gboolean UnselectCB(struct Block *key, struct Block *val,
                  gpointer data) {
    UnselectBlock(key);
    return FALSE; // Keep traversing.
}

static void UnselectAllBlocks(struct Page *page) {

    DASSERT(page);
    g_tree_foreach(page->selectedBlocks, (GTraverseFunc) UnselectCB, 0);
}

static void SelectBlock(struct Block *b) {

    DASSERT(b);
    // Put it on the page selected stack.
    struct Page *page = b->page;
    DASSERT(page);
    g_tree_insert(page->selectedBlocks, b, b);
    gtk_widget_set_name(b->grid, "selectedBlock");
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


static inline struct Connector *FindConnectTo(
        struct Connector *from, struct Block *toBlock) {

    DASSERT(from);
    DASSERT(from->block != toBlock);

    switch(from->type) {
        case CT_INPUT:
            return &toBlock->output;
        case CT_OUTPUT:
            return &toBlock->input;
        case CT_GET:
            return &toBlock->set;
        case CT_SET:
            return &toBlock->get;
#ifdef DEBUG
        default:
            DASSERT(0, "missing case");
            return 0;
#endif
    }
    return 0;
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
    return false;
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
 * of the distance to the cubic Bezier spline control point, that we use
 * to draw the lines with using cairo_curve_to().
 */

#define CONNECT_IMAGE_LEN   (32)

#define CONNECT_LEN  ((double) CONNECT_IMAGE_LEN * 3.2)


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
        double x0, double y0, double *x1, double *y1,
        double len) {

    DASSERT(c);

    // TODO: deal with rotated blocks.

    switch(c->type) {
        case CT_INPUT:
            *x1 = x0 - len;
            *y1 = y0;
            break;
        case CT_OUTPUT:
            *x1 = x0 + len;
            *y1 = y0;
            break;
        case CT_GET:
            *x1 = x0; 
            *y1 = y0 + len;
            break;
        case CT_SET:
            *x1 = x0;
            *y1 = y0 - len;
    }
}


static void DrawConnection(struct Connector *c0, struct Connector *c1,
        cairo_surface_t *s) {

    double x0, y0, x1, y1, x2, y2, x3, y3;

    GetConnectionPoint(c0, &x0, &y0);
    GetConnectionPoint(c1, &x3, &y3);

    // We use Bezier control points that are half of the length of the
    // connection.  Otherwise in the case of short connection lengths
    // we can get loops and kinks in the connecting line.
    double l = (x0 - x3)*(x0 - x3) + (y0 - y3)*(y0 - y3);
    l = sqrt(l)/2.0;
    // Or if that is too large we use a limiting length.
    if(l > CONNECT_LEN) l = CONNECT_LEN;

    GetConnectionPoints(c0, x0, y0, &x1, &y1, l);
    GetConnectionPoints(c1, x3, y3, &x2, &y2, l);
    double r, g, b, a;
    GetConnectionColor(c0, &r, &g, &b, &a);

    // TODO: if this is called in a loop we may want to keep the cr
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


// Draw onto the cairo surface page->newDrawSurface the line as the mouse is
// pressed and the pointer moves.  The line is from the connector widget
// that is referred to with page->from (x0,y0) and to the pointer position
// (x1,y1).
//
void DrawDragLine(struct Page *page) {

    // GTK (the piece of shit) seems to be setting errno all the time.
    errno = 0;

    DASSERT(page);
    DASSERT(page->newDrawSurface);
    DASSERT(page->from);
    DASSERT(page->from->block);

    double x0, y0, x1, y1;
    GetConnectionPoint(page->from, &x0, &y0);
    GtkAllocation alloc;
    gtk_widget_get_allocation(page->from->widget, &alloc);
    x1 = page->pointer_x + alloc.x;
    y1 = page->pointer_y + alloc.y;

    // draw onto page->newDrawSurface
    cairo_t *cr = cairo_create(page->newDrawSurface);
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
static bool gotSelectBox = false;


/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean drawLayout(GtkWidget *widget, cairo_t *cr,
        struct Page *page)
{
    DASSERT(page);

    bool needOldLineRedraw = false;

    cairo_surface_t *n = GetSurface(page, widget, page->newDrawSurface);

    if(n) {
        // we resized or created new; so now these surfaces have nothing
        // on them.
        page->newDrawSurface = n;
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
        cairo_set_source_surface(cr, page->newDrawSurface, 0, 0);
        cairo_paint(cr);
    } else if(gotSelectBox) {
        cairo_set_source_surface(cr, page->newDrawSurface, 0, 0);
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
                "background-color: rgba(83,138,250,0.0);\n"
                "border: 2px solid black;\n"
                "}\n"
            "#disabledBlock {\n"
                "background-color: rgba(83,138,250,0.0);\n"
                "border: 2px solid rgba(118,162,247,0.3);\n"
                "}\n"
            "#selectedBlock {\n"
                "background-color: rgba(83,138,250,0.0);\n"
                "border: 2px solid rgba(234,12,234,0.8);\n"
                "}\n"

            "#block > #get,\n"
            "#block > #set,\n"
            "#block > #input,\n"
            "#block > #output {\n"
                "background-color: rgba(147,176,223,0.5);\n"
                "border: 1px solid rgb(118,162,247);\n"
                "font-size: 130%;\n"
                "}\n"
            "#selectedBlock > #get,\n"
            "#selectedBlock > #set,\n"
            "#selectedBlock > #input,\n"
            "#selectedBlock > #output {\n"
                "background-color: rgba(76,230,228,0.6);\n"
                "border: 1px solid rgba(8,2,7,0.1);\n"
                "}\n"

            "#disabledBlock > #label {\n"
                "background-color: rgba(83,138,250,0.1);\n"
                "border: 1px solid rgba(118,162,247,0.1);\n"
                "color: rgba(218,242,247,0.5);\n"
                "}\n"
            "#selectedBlock > #label {\n"
                "background-color: rgba(76,230,228,0.6);\n"
                "color: rgba(88,88,88,0.6);\n"
                "}\n"
            "#block > #label {\n"
                "background-color: rgba(156,190,224,0.6);\n"
                "color: black;\n"
                "}\n"
            "#label {\n"
                "font-size: 130%;\n"
                "}\n"
            , -1, &error);

    g_object_unref (provider);
}


// Push a block to the top of the page layout.
//
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


static gboolean MoveBlockCB(struct Block *b,
        struct Block *val, gint *dxy) {

    b->x += dxy[0];
    b->y += dxy[1];
    gtk_layout_move(b->page->layout, b->container, b->x, b->y);
    return FALSE; // FALSE == keep going
}


static void MoveSelectedBlocks(struct Page *page, gint dx, gint dy) {

    gint dxy[2] = { dx, dy };
    g_tree_foreach(page->selectedBlocks, (GTraverseFunc) MoveBlockCB, dxy);
}


static gboolean SelectBlockInRect(struct Block *b, gint *rect) {

    gint w = gtk_widget_get_allocated_width(b->container);
    gint h = gtk_widget_get_allocated_height(b->container);

    if( (b->x <= rect[0] + rect[2]) && ((b->x + w) >= rect[0]) &&
        (b->y <= rect[1] + rect[3]) && ((b->y + h) >= rect[1]))
        SelectBlock(b);

ERROR();
    return FALSE; // FALSE == keep going
}


static bool BlockSelected(struct Block *b) {

    if(g_tree_lookup(b->page->selectedBlocks, b))
        return true;
    return false;
}


static GtkMenu *popupMenu = 0;
static struct Block *popupBlock = 0;


static
void QueueConnectionsDraw(GtkLayout *layout) {

    DASSERT(layout);

    gint w = gtk_widget_get_allocated_width(GTK_WIDGET(layout));
    gint h = gtk_widget_get_allocated_height(GTK_WIDGET(layout));
    /* Now invalidate the affected region of the drawing area. */
    gtk_widget_queue_draw_area(GTK_WIDGET(layout), 0, 0, w, h);
}



static inline void BlockDestroy(struct Block *b) {

    DASSERT(b);
    struct Page *page = b->page;
    DASSERT(page);
    GtkLayout *layout = page->layout;
    DASSERT(layout);

    UnselectBlock(b);

    // TODO: Maybe use different data structure than an array to keep the
    // connections in.  This may not be too bad; it is order O(n).  But
    // it is a little complex, and hard to test with the GUI.

    // Remove all connections that point to this block.
    size_t oldNum = page->numConnections;
    size_t newNum = 0;
    for(size_t i=0; i<oldNum;++i) {
        while(i<oldNum &&
                (page->connections[i].from->block == b ||
                page->connections[i].to->block == b))
            // while not a good one go to next i:
            ++i;
        if(i < oldNum) {
            // i is a good one.
            //
            // We shift the good array element back to the
            // element of the current found good ones.
            //
            // If the structure of Connection changes
            // this way of copying will still work.
            memcpy(page->connections + newNum,
                    page->connections + i,
                    sizeof(*page-> connections));
            // Got one, count it.
            ++newNum;
        }
    }
    //
    if(newNum) {
        if(newNum != oldNum) {
#ifdef DEBUG
            // zero-out the part that will be freed which is on the end of
            // the array.
            memset(&page->connections[newNum], 0, (oldNum-newNum)
                    *sizeof(*page->connections));
#endif
            page->connections = realloc(page->connections,
                    newNum*sizeof(*page->connections));
            ASSERT(page->connections,
                    "realloc(%p, %zu) failed", page->connections,
                    newNum*sizeof(*page->connections));
            page->numConnections = newNum;
        }
    } else if(oldNum) {
#ifdef DEBUG
        memset(page->connections, 0, oldNum*sizeof(*page->connections));
#endif
        free(page->connections);
        page->connections = 0;
        page->numConnections = 0;
    }

    gtk_widget_destroy(GTK_WIDGET(b->container));
  
    // Remove from the Page stack of blocks.
    struct Block *lb = page->blocks;
    struct Block *prev = 0;
    while(lb && lb != b) {
        prev = lb;
        lb = lb->next;
    }
    DASSERT(lb);
    if(prev)
        prev->next = b->next;
    else
        page->blocks = b->next;

#ifdef DEBUG
    memset(b, 0, sizeof(*b));
#endif
    free(b);

    RedrawOldLines(page);

    QueueConnectionsDraw(layout);

    // TODO: remove inputs and outputs.
}


static void RemovePopupBlock(GtkWidget *widget,
        GdkEvent *e, gpointer data) {
    BlockDestroy(popupBlock);
    popupBlock = 0;
}


static gboolean
BlockEnterCB(GtkWidget *widget, GdkEventButton *e, struct Block *b) {

    DASSERT(b);
    struct Page *page = b->page;
    DASSERT(page);

    WARN();

    if(!b->page->from || !waitingForConnectEnter ||
            b->page->from->block == b)
        return TRUE;


    // We finish the process of making a connection to between 2 blocks.

    // 1. find the connector that we go to.
    struct Connector *to = FindConnectTo(page->from, b);
    if(!to)
        // We could not find the connector to go to, but maybe the next
        // widget can determine where to connect to.
        return FALSE; // FALSE = send event to next child widget

    // We got a connect release just before this.  We now setup
    // a connection.
    //
    // TODO: add more here to record the connection.
    DrawConnection(page->from, to, page->oldLines);
    AddConnection(page->from, to);

    // Reset for next connection
    page->from = 0;
    waitingForConnectEnter = false;
    QueueConnectionsDraw(page->layout);

    return TRUE; // TRUE = do not go to next widget
}

static gboolean
BlockButtonCB(GtkWidget *widget, GdkEventButton *e, struct Block *b) {

    errno = 0;
    static bool gotPress = false;
    static gint xOffset = 0, yOffset = 0;

    DASSERT(b);
    struct Page *page = b->page;
    DASSERT(page);


    switch(e->type) {
    
        case GDK_BUTTON_PRESS:
        {
            if(e->button == BLOCK_POPUP_BUTTOM) {

                popupBlock = b;
                UnselectAllBlocks(page);
                SelectBlock(b);
                gtk_menu_popup_at_pointer(popupMenu, (GdkEvent *) e);
                return TRUE; // Event stops here.
            }

            if(e->button != BLOCK_MOVE_BUTTON)
                break;

            if(!BlockSelected(b)) {

                if(!(e->state & GDK_SHIFT_MASK))
                    // Holding down shift key will not unselect any
                    // previously selected blocks.
                    UnselectAllBlocks(page);

                SelectBlock(b);
            }

            SetCursor(GTK_WIDGET(page->layout), moveCursor);
            xOffset = e->x_root;
            yOffset = e->y_root;
            PopForward(page->layout, b);
            gotPress = true;
            gtk_grab_add(widget);
            return TRUE; // Event stops here.
        }

        case GDK_MOTION_NOTIFY:

            // We get e->x and e->y that are relative to the widget where
            // this event first was received by a callback and it may not
            // be this Layout widget; so we must use the x_root and y_root
            // position values, which are positions relative to root
            // (whatever the hell that is).
            if(waitingForConnectEnter)
                ClearNewConnectionDraw(page);

            if(!gotPress) break;

            blockMoved = true;
 
            MoveSelectedBlocks(page,
                    ((gint) e->x_root) - xOffset,
                    ((gint) e->y_root) - yOffset);

            xOffset = e->x_root;
            yOffset = e->y_root;

            return TRUE; // Event stops here.

        case GDK_BUTTON_RELEASE:
        {
            SetCursor(GTK_WIDGET(page->layout), 0);

            if(e->button != BLOCK_MOVE_BUTTON) break;

            blockMoved = true;

            gtk_grab_remove(widget);

            gotPress = false;

            return TRUE; // Event stops here.
        }

        default:
            break;
    }

    return FALSE; // FALSE = go to next widget
}

static GdkDisplay *display = 0;
static GdkSeat *seat = 0;
//static GdkDevice *pointerDevice = 0;


static gboolean
ConnectPressCB(GtkWidget *w, GdkEventButton *e, struct Connector *c) {

    errno = 0;

    DSPEW("type=%s", gtk_widget_get_name(w) );

    // There seems to be a bug such that sometime the event type is not
    // GDK_BUTTON_PRESS.
    //DASSERT(e->type == GDK_BUTTON_PRESS);
    
    if(e->type != GDK_BUTTON_PRESS) return FALSE;

    if(e->button != BLOCK_CONNECT_BUTTON) return FALSE;

    // HERE LANCE
    if(0) gdk_seat_ungrab(seat);

    DASSERT(c);
    DASSERT(c->block);
    waitingForConnectEnter = false;
    c->block->page->from = c;

    DASSERT(c->block->page);
    // page->pointer_x|y be the pointer position relative to the layout.
    c->block->page->pointer_x = e->x + c->block->x;
    c->block->page->pointer_y = e->y + c->block->y;

    c->block->page->from = c;

    QueueConnectionsDraw(c->block->page->layout);

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

        QueueConnectionsDraw(c->block->page->layout);
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
    DASSERT(page->newDrawSurface);

    // Clear the new line surface
    cairo_t *cr = cairo_create(page->newDrawSurface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
    cairo_paint(cr);
    cairo_destroy(cr);
    QueueConnectionsDraw(page->layout);

    if(!page->from) {
        return TRUE;
    }

    if(page->from)
        waitingForConnectEnter = true;

    return TRUE; // TRUE = do not go to next widget
}

static gboolean
ConnectEnterCB(GtkWidget *w, GdkEvent *e, struct Connector *to) {

WARN();

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
    QueueConnectionsDraw(page->layout);

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

/*
     Block with 0 degrees rotation:

 ***************************************
 *                set                  *
 *                                     *
 * input                        output *
 *                                     *
 *                get                  *
 ***************************************
 */

static void DrawConnectImageSurface(struct Connector *c, uint32_t rot) {

    DASSERT(c);
    cairo_surface_t *s = c->surface;
    DASSERT(s);

    double r, g, b, a;
    GetConnectionColor(c, &r, &g, &b, &a);
    // override the alpha.
    //a = 0.4;

    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_set_line_width(cr, 6);


    cairo_translate(cr, 16, 16);

    if(c->type == CT_INPUT || c->type == CT_OUTPUT)
        cairo_rotate(cr, (rot + 2) * M_PI/2.0);
    else // if(c->type == CT_GET || c->type == CT_SET)
        cairo_rotate(cr, (rot + 3) * M_PI/2.0);

    cairo_rotate(cr, M_PI);

    cairo_translate(cr, -16, -16);

    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, 31, 15);
    cairo_line_to(cr, 0, 31);


    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
    cairo_destroy(cr);
}


//
// rot - is rotation is units a 90 degrees up to 270 degrees and then back
// to zero degrees.
//
static inline cairo_surface_t *
CreateConnectImageSurface(struct Connector *c, uint32_t rot) {

    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            CONNECT_IMAGE_LEN, CONNECT_IMAGE_LEN);
    DASSERT(s);
    DASSERT(c->surface == 0);
    c->surface = s;
    DrawConnectImageSurface(c, rot);

    return s;
}



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

    cairo_surface_t *surface = CreateConnectImageSurface(c, 0);

    GtkWidget *img = gtk_image_new_from_surface(surface);
    // Decreases the reference count on surface by one.  The GTK image will
    // add its' own reference.
    cairo_surface_destroy(surface);

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




static struct Block *BlockCreate(struct Page *page,
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
    // Add to the Page stack of blocks.
    block->next = page->blocks;
    page->blocks = block;

    GtkWidget *grid = gtk_grid_new();
    block->grid = grid;
    GtkWidget *ebox = gtk_event_box_new();

    block->container = ebox;
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
    g_signal_connect(ebox, "enter-notify-event",
        G_CALLBACK(BlockEnterCB), block);

    //gtk_widget_set_name(ebox, "get");
    gtk_widget_show(ebox);
    gtk_container_add(GTK_CONTAINER(ebox), grid);
    gtk_layout_put(layout, ebox, x, y);


    // The Grid of a block
    //
    // 0,0  1,0  2,0
    // 
    // 0,1  1,1  2,1
    //
    // 0,2  1,2  2,2
    //
    // 0,3  1,3  2,3
    //
    // 0,4  1,4  2,4

    MakeBlockLabel(grid, name, 1, 1, 1, 1);
    MakeBlockLabel(grid, "block type 923rj", 1, 2, 1, 1);
    MakeBlockLabel(grid, "Boston", 1, 3, 1, 1);

    MakeBlockConnector(grid, "input", "input.png", 0, 0, 1, 5, block, &block->input);
    MakeBlockConnector(grid, "output", "output.png", 2, 0, 1, 5, block, &block->output);
    MakeBlockConnector(grid, "set", "set.png", 1, 0, 1, 1, block, &block->set);
    MakeBlockConnector(grid, "get", "get.png", 1, 4, 1, 1, block, &block->get);

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
            if(e->button == SELECT_BUTTON) {
                xOffset = e->x;
                yOffset = e->y;
                UnselectAllBlocks(page);
                gotSelectBox = true;
            }

            if(e->button != BLOCK_CREATE_BUTTON) break;

            // Create a new block and then move it.
            char *name = strdup("block Name larger block Name ya");
            movingBlock = BlockCreate(page, layout, name, e->x, e->y);
            UnselectAllBlocks(page);
            SelectBlock(movingBlock);

            xOffset = e->x_root;
            yOffset = e->y_root;

            SetCursor(GTK_WIDGET(layout), moveCursor);
            WriteStatus("created block \"%s\"", name);
            free(name);
            break;
        }

        case GDK_MOTION_NOTIFY:

            if(gotSelectBox) {

                cairo_t *cr = cairo_create(page->newDrawSurface);
                cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
                // clear the surface
                cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
                cairo_paint(cr);

                cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, 0.4);
                cairo_rectangle (cr, xOffset, yOffset,
                        e->x - xOffset, e->y - yOffset);
                cairo_fill(cr);
                cairo_destroy(cr);
    
                gint w = gtk_widget_get_allocated_width(GTK_WIDGET(layout));
                gint h = gtk_widget_get_allocated_height(GTK_WIDGET(layout));
                /* Now invalidate the affected region of the drawing area. */
                gtk_widget_queue_draw_area(GTK_WIDGET(layout), 0, 0, w, h);

                break;
            }


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

            if(gotSelectBox) {
                gotSelectBox = false;

                gint rect[4] = {
                    xOffset, yOffset, // x, y
                    e->x - xOffset, e->y - yOffset // width, height
                };

                // Make sure that the width of the rectangle (rect) is
                // positive.
                if(rect[2] < 0) {
                    rect[2] = xOffset - e->x;
                    rect[0] = e->x;
                }

                // Make sure that the height of the rectangle (rect) is
                // positive.
                if(rect[3] < 0) {
                    rect[3] = yOffset - e->y;
                    rect[1] = e->y;
                }

                for(struct Block *bl = page->blocks; bl; bl = bl->next)
                        SelectBlockInRect(bl, rect);

                cairo_t *cr = cairo_create(page->newDrawSurface);
                cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
                // clear the surface
                cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
                cairo_paint(cr);
                cairo_destroy(cr);

                QueueConnectionsDraw(layout);
            }


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



static GtkNotebook *noteBook = 0;


static inline void MakeWorkArea(struct Page *page) {

    GtkBuilder *b = gtk_builder_new_from_resource(
            "/quickstreamBuilder/qsb_workArea_res.ui");

    page->layout = GTK_LAYOUT(gtk_builder_get_object(b, "workArea"));

    Connect(b, "workArea", "draw", drawLayout, page);
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

static gint BlockSelectCompareCB(gconstpointer a, gconstpointer b) {

    if(a > b)
        return 1;
    if(a < b)
        return -1;
    return 0;
}


// TODO: free(page);


static gboolean NewTab(GtkWidget *w, gpointer data) {

    struct Page *page = calloc(1, sizeof(*page));
    ASSERT(page, "calloc(1, %zu) failed", sizeof(*page));

    page->selectedBlocks = g_tree_new(BlockSelectCompareCB);

    MakeWorkArea(page);
    return TRUE;
}


static inline void
setup_widget_connections(void) {

    display = gdk_display_get_default();
    seat = gdk_display_get_default_seat(display);
    //pointerDevice = gdk_seat_get_pointer(seat);

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

    
    GtkBuilder *popupBuilder = gtk_builder_new_from_resource(
            "/quickstreamBuilder/qsb_popup_res.ui");
    popupMenu = GTK_MENU(gtk_builder_get_object(popupBuilder, "popupMenu"));
    Connect(popupBuilder, "remove", "activate", RemovePopupBlock, 0);

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

#if 0

static inline void EventLoop(void) {

    GdkDisplay *display = gdk_display_get_default();

    ASSERT(GDK_IS_X11_DISPLAY(display));

    Display *xdpy = GDK_DISPLAY_XDISPLAY(display);
    // X11 Display connection number, file descriptor.
    int fd = ConnectionNumber(xdpy);

    bool running = true;

#ifndef GDK_WINDOWING_X11
#  error "FAIL"
#endif

    while(running) {

        fd_set rfds;
        int retval;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        retval = select(1+fd, &rfds, 0, 0, 0);
WARN("SELECT POP");
        ASSERT(retval != -1);

        while(gtk_events_pending())
            gtk_main_iteration();
    }
}


#endif

int main(int argc, char *argv[]) {

    signal(SIGSEGV, catcher);
    signal(SIGABRT, catcher);

    gtk_init(&argc, &argv);

    CSS();

    setup_widget_connections();

    gtk_main(); 
    //EventLoop();

    DSPEW("xmlReadFile=%p", xmlReadFile);
    return 0;
}
