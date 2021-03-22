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

#include "../include/quickstream/app.h"
// TODO: Looks like the definition of enum QsParameterType is needed for
// builder.h.  This breaks the idea of keeping builder.h not dependent on
// block.h.
#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"

#include "../lib/debug.h"

#include "qsb.h"


// We have one app.
// TODO: add more apps.  Maybe not.
//
struct QsGraph *graph = 0;

struct Block *movingBlock = 0;
// The position of the mouse pointer at the last mouse event.
static double x_0, y_0;


GtkNotebook *noteBook = 0;
static int tabCreateCount = 0;
static GtkWidget *window = 0;

// List of cursors with images:
// https://developer.gnome.org/gdk3/stable/gdk3-Cursors.html#GdkCursorType
static GdkCursor *moveCursor = 0;


static bool haveSelectBox = false;
static gdouble selectBoxX, selectBoxY;


static inline void SetCursor(GtkWidget *w, GdkCursor *cursor) {
    gdk_window_set_cursor(gtk_widget_get_window(w), cursor);
}


static inline GdkCursor *GetCursor(const char *type) {
    return gdk_cursor_new_from_name(gdk_display_get_default(), type);
}


static inline cairo_surface_t *
MakeNewLayoutSurface(GtkWidget *widget, cairo_surface_t *old,
        gint w, gint h) {

    if(old)
        cairo_surface_destroy(old);

    GdkWindow *win = gtk_widget_get_window(widget);
    cairo_surface_t *new = gdk_window_create_similar_surface(win,
            CAIRO_CONTENT_COLOR_ALPHA, w, h);
    cairo_t *cr = cairo_create(new);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    return new;
}


// This does work.  The position is relative to the root window.
//
static void GetPointer(double *x, double *y) {

    GdkDevice *mouse_device;

#if GTK_CHECK_VERSION (3,20,0)
    GdkSeat *seat =
        gdk_display_get_default_seat(gdk_display_get_default());
    mouse_device = gdk_seat_get_pointer(seat);
#else
    GdkDeviceManager *devman =
        gdk_display_get_device_manager(gdk_display_get_default());
    mouse_device = gdk_device_manager_get_client_pointer(devman);
#endif
    GdkScreen *screen = gdk_screen_get_default();
    gdk_device_get_position_double(mouse_device, &screen, x, y);
}


// We have this stupid surface re-allocator because the GTK layout widget
// will not receive configure events, which are when the widget window
// resizes.
//
// Return true if the surfaces are allocated or reallocated.
//
// We use more than one surface in drawing the page, so we needed the
// forceMakeNew flag to cause this to reallocate the surface even when
// the page
//
static inline bool
GetLayoutSurfaces(struct Page *page, GtkWidget *widget) {

    gint w = gtk_widget_get_allocated_width(widget);
    gint h = gtk_widget_get_allocated_height(widget);

    if(page->w == w && page->h == h)
        // nothing to do.
        return false;

    page->w = w;
    page->h = h;

    // reallocate the 2 page layout Cairo drawing surfaces.
    page->newDrawSurface =
        MakeNewLayoutSurface(widget, page->newDrawSurface, w, h);
    page->oldLines =
        MakeNewLayoutSurface(widget, page->oldLines, w, h);

    return true;
}


// Push a block to the top of the page layout.
//
static void PopForwardBlock(GtkLayout *layout, GtkWidget *ebox,
        double x, double y) {

    // 1. get one more reference to the widget; otherwise
    //    the widget will be destroyed in the next call.
    g_object_ref(G_OBJECT(ebox));

    // 2. remove the widget which removes one reference
    gtk_container_remove(GTK_CONTAINER(layout), ebox);
    // Now we have at least one reference.
    // 3. re-add the widget which will take ownership of the
    //    one reference.
    gtk_layout_put(layout, ebox, x, y);
}


static
gboolean
WorkArea_drawCB(GtkWidget *layout, cairo_t *cr, struct Page *page) {


    if(GetLayoutSurfaces(page, layout)) {
        // We have newly allocated surfaces.  The page->oldLines
        // surface needs to have all the current connects drawn on it.

        DSPEW("draw all existing connection lines"); // TODO:
        // TODO: draw all connection lines on page->oldLines.
    }


    // 1. clear the current surface.
    cairo_set_source_rgb(cr, 224/255.0, 233/255.0, 244/255.0);
    cairo_paint(cr);


    // 2. draw selection boxes or new lines while the mouse pointer moves.
    // There can be no layout resizing in these cases.
    if(haveSelectBox || fromConnector) {
        cairo_set_source_surface(cr, page->newDrawSurface, 0, 0);
        cairo_paint(cr);
    }


    // 3. draw the saved block connecting lines that we know so far.
    cairo_set_source_surface(cr, page->oldLines, 0, 0);
    cairo_paint(cr);

    return FALSE; // FALSE == Call other callbacks
}


static inline
gboolean SelectBlockInRect(struct Block *b, gint *rect) {

    GtkAllocation alloc;
    gtk_widget_get_allocation(b->ebox, &alloc);

    //gint w = gtk_widget_get_allocated_width(b->ebox);
    //gint h = gtk_widget_get_allocated_height(b->ebox);

    if( (alloc.x <= rect[0] + rect[2]) &&
        ((alloc.x + alloc.width) >= rect[0]) &&
        (alloc.y <= rect[1] + rect[3]) &&
        ((alloc.y + alloc.height) >= rect[1]))
        SelectBlock(b);

    return FALSE; // FALSE == keep going
}


static gboolean MoveBlockCB(struct Block *b,
        struct Block *val, double *dxy) {

    gtk_layout_move(GTK_LAYOUT(gtk_widget_get_parent(b->ebox)),
            b->ebox, b->x += dxy[0], b->y += dxy[1]);
    return FALSE; // FALSE == keep going
}


static void MoveSelectedBlocks(struct Page *page, double dx, double dy) {

    double dxy[2] = { dx, dy };
    g_tree_foreach(page->selectedBlocks, (GTraverseFunc) MoveBlockCB, dxy);
}


static gboolean WorkArea_buttonReleaseCB(GtkWidget *layout,
        GdkEventButton *e, struct Page *page) {


    if(e->type != GDK_BUTTON_RELEASE) {
        // This should not happen.
        ASSERT(0, "Did not get GDK_BUTTON_RELEASE event");
        return FALSE; // FALSE = go to next widget
    }
    if(e->button != CREATE_BLOCK_BUTTON)
        return FALSE; // FALSE = go to next widget

    if(haveSelectBox) {

        DASSERT(movingBlock == 0);
 
        if(!(e->state & GDK_SHIFT_MASK))
            UnselectAllBlocks(page);

        gint rect[4] = {
            selectBoxX, selectBoxY, // x, y
            e->x - selectBoxX, e->y - selectBoxY // width, height
        };

        // Make sure that the width of the rectangle (rect) is positive.
        if(rect[2] < 0) {
            rect[2] = selectBoxX - e->x;
            rect[0] = e->x;
        }

        // Make sure that the height of the rectangle (rect) is
        // positive.
        if(rect[3] < 0) {
            rect[3] = selectBoxY - e->y;
            rect[1] = e->y;
        }

        for(struct Block *bl = page->blocks; bl; bl = bl->next)
            SelectBlockInRect(bl, rect);

        haveSelectBox = false;
        gint w = gtk_widget_get_allocated_width(layout);
        gint h = gtk_widget_get_allocated_height(layout);
        /* Now invalidate the affected region of the drawing area to
         * remove the selection rectangle that we drew. */
        gtk_widget_queue_draw_area(layout, 0, 0, w, h);
    }


    if(!movingBlock) return TRUE;

    movingBlock = 0;
    SetCursor(layout, 0);

    return TRUE; // Eat this event at this widget
}


const static double lineWidth = 4.2;


static inline void
DrawConnectionDragLine(GdkEventButton *e, struct Page *page) {

    GtkWidget *layout = page->layout;

    GetLayoutSurfaces(page, layout);
    DASSERT(page->newDrawSurface);

    // Draw the current connection line that the user is currently
    // dragging.  Call this block of code: DrawDragLine.
    double x0=0, y0=0, x1, y1;
// TODO: this:
//GetConnectionPoint(fromConnector, &x0, &y0);
    GtkAllocation alloc;
    gtk_widget_get_allocation(layout, &alloc);
    GetWidgetRootXY(layout, &x1, &y1);

    if(e) {
        x1 = e->x_root - x1;
        y1 = e->y_root - y1;
    } else {
        double px, py;
        GetPointer(&px, &py);
        x1 = px - x1;
        y1 = py - y1;
    } 

    // draw onto page->newDrawSurface
    cairo_t *cr = cairo_create(page->newDrawSurface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    // clear the surface
    cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
    cairo_paint(cr);

    // Set the line color
    double r=1.0, g=0, b=0, a=0.4;
// TODO: this:
//GetConnectionLineColor(fromConnector, &r, &g, &b, &a);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_set_line_width(cr, lineWidth);
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x1, y1);
    cairo_stroke(cr);
    cairo_destroy(cr);

    // The 'draw' event callback, WorkArea_drawCB(), will paste
    // together all the needed surfaces into the layout widget.
    gtk_widget_queue_draw_area(layout, 0, 0, page->w, page->h);
}


static gboolean WorkArea_mouseMotionCB(GtkLayout *layout,
        GdkEventButton *e, struct Page *page) {

    if(e->type != GDK_MOTION_NOTIFY) {
        // This should not happen.
        ASSERT(0, "Did not get GDK_MOTION_NOTIFY event");
        return FALSE; // FALSE = go to next widget
    }

    // Because we only use one pointer/mouse we can't have a selection and
    // a connection being and move a block all at the same time.  TODO:
    // What happens if a user clicks two mouse buttons at the same time?
    // For now fuck it, I do not want to think about it; but at least see
    // it happen in DEBUG.
    DASSERT(!haveSelectBox || !fromConnector || !movingBlock);


    if(fromConnector) {
        DrawConnectionDragLine(e, page);
        return TRUE; // TRUE = eat the event
    }

    if(haveSelectBox) {
        DASSERT(movingBlock == 0);
        GetLayoutSurfaces(page, GTK_WIDGET(layout));

        cairo_t *cr = cairo_create(page->newDrawSurface);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        // clear the surface
        cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
        cairo_paint(cr);

        cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, 0.4);
        cairo_rectangle (cr, selectBoxX, selectBoxY,
                e->x - selectBoxX, e->y - selectBoxY);
        cairo_fill(cr);
        cairo_destroy(cr);
    
        /* Now invalidate the affected region of the drawing area. */
        gtk_widget_queue_draw_area(GTK_WIDGET(layout),
                0, 0, page->w, page->h);
        return TRUE; // TRUE eat this event
    }

    if(!movingBlock) return TRUE;

    MoveSelectedBlocks(page, e->x_root - x_0, e->y_root - y_0);
    x_0 = e->x_root;
    y_0 = e->y_root;

    return TRUE; // Eat this event at this widget
}


void StartDragingConnection(struct Page *page) {

    DASSERT(fromConnector);
    DrawConnectionDragLine(0, page);
}


void StopDragingConnection(struct Page *page) {

    DASSERT(fromConnector);
    // the user gave up on making a connection by leaving the layout
    // widget, or clicking in the wrong place of something.
    //        
    // clear the page->newDrawSurface
    cairo_t *cr = cairo_create(page->newDrawSurface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
    cairo_paint(cr);

    gtk_widget_queue_draw_area(page->layout, 0, 0, page->w, page->h);

    // We no longer have a connection being made by the user.
    fromConnector = 0;
}


static gboolean WorkArea_buttonPressCB(GtkLayout *layout,
        GdkEventButton *e, struct Page *page) {


    if(e->type != GDK_BUTTON_PRESS) {
        // This should not happen; but it does.
        // ya. GTK sucks.
        //ASSERT(0, "Did not get GDK_BUTTON_PRESS event");
        return FALSE; // FALSE = go to next widget
    }


    DASSERT(haveSelectBox == false);
    
    if(e->button != CREATE_BLOCK_BUTTON)
        return FALSE; // FALSE = go to next widget

    const char *blockFile = GetSelectedBlockFile();

    if(!(e->state & GDK_SHIFT_MASK) &&
            (!movingBlock || movingBlock->isSelected == false))
        UnselectAllBlocks(page);

    double xi, yi;

    if(!movingBlock) {
        xi = 2*CONNECTOR_THICKNESS;
        yi = 2*CONNECTOR_THICKNESS;
        if(blockFile)
            movingBlock = AddBlock(page, layout, blockFile,
                    e->x - xi, e->y - yi);

        if(!movingBlock) {
            haveSelectBox = true;
            selectBoxX = e->x;
            selectBoxY = e->y;
            return TRUE; // eat event
        }
    } else {

        // The block was pressed and set movingBlock.
        double xb, yb;
        GetWidgetRootXY(movingBlock->ebox, &xb, &yb);
        xi = e->x_root - xb;
        yi = e->y_root - yb;
    }
    double xl, yl;
    GetWidgetRootXY(GTK_WIDGET(layout), &xl, &yl);
    xi += xl;
    yi += yl;

    x_0 = e->x_root;
    y_0 = e->y_root;

    SetCursor(GTK_WIDGET(layout), moveCursor);

    PopForwardBlock(layout, movingBlock->ebox,
            e->x_root - xi, e->y_root - yi);

    SelectBlock(movingBlock);

    return TRUE; // Eat this event at this widget
}


static gint BlockSelectCompareCB(gconstpointer a, gconstpointer b) {

    if(a > b)
        return 1;
    if(a < b)
        return -1;
    return 0;
}


static inline void MakeWorkArea(struct Page *page) {

    GtkBuilder *b = gtk_builder_new_from_resource(
            "/quickstreamBuilder/qsb_workArea_res.ui");

    char tagName[64];
    if(tabCreateCount) {
        ++tabCreateCount;
        snprintf(tagName, 64, "Untitled_%d", tabCreateCount);
    } else {
        ++tabCreateCount;
        strcpy(tagName, "Untitled");
    }

    GtkLayout *layout = GTK_LAYOUT(gtk_builder_get_object(b, "workArea"));
    page->layout = GTK_WIDGET(layout);

    gint rt = gtk_notebook_append_page(noteBook, GTK_WIDGET(layout),
            gtk_label_new(tagName));
    ASSERT(rt >= 0);

    //gtk_widget_add_events(page->layout, GDK_ENTER_NOTIFY_MASK);
    //gtk_widget_add_events(page->layout, GDK_LEAVE_NOTIFY_MASK);



    g_signal_connect(GTK_WIDGET(layout), "draw",
            G_CALLBACK(WorkArea_drawCB), page/*userData*/);

    g_signal_connect(GTK_WIDGET(layout), "button-press-event",
            G_CALLBACK(WorkArea_buttonPressCB), page/*userData*/);

    g_signal_connect(GTK_WIDGET(layout), "motion-notify-event",
            G_CALLBACK(WorkArea_mouseMotionCB), page/*userData*/);

    g_signal_connect(GTK_WIDGET(layout), "button-release-event",
            G_CALLBACK(WorkArea_buttonReleaseCB), page/*userData*/);

    //g_signal_connect(GTK_WIDGET(layout), "enter-notify-event",
    //        G_CALLBACK(WorkArea_enterCB), page/*userData*/);


    // We are done with this builder
    g_object_unref(G_OBJECT(b));
}


static gboolean NewTab(GtkWidget *w, gpointer data) {

    struct Page *page = calloc(1, sizeof(*page));
    ASSERT(page, "calloc(1,%zu) failed", sizeof(*page));
    // TODO: free(page) somewhere.

    page->selectedBlocks = g_tree_new(BlockSelectCompareCB);
    // Set width and height to invalid values.
    page->w = -1101;
    page->h = -1101;

    page->connectorsPopover.label = gtk_label_new("Connector Help Text");
    page->connectorsPopover.container = gtk_frame_new(0);
    gtk_container_add(GTK_CONTAINER(page->connectorsPopover.container),
            page->connectorsPopover.label);
    gtk_widget_show(page->connectorsPopover.label);
    gtk_widget_set_name(page->connectorsPopover.container, "popover");
    // initialize the layout position to a value that is invalid
    gtk_widget_set_size_request(page->connectorsPopover.container,
            MIN_POPOVER_WIDTH, CONNECTOR_THICKNESS);

    MakeWorkArea(page);

    return TRUE;
}


static inline void
setup_widgets(void) {

    moveCursor = GetCursor("grabbing");

    GtkBuilder *b = gtk_builder_new_from_resource(
            "/quickstreamBuilder/qsb_res.ui");


    window = GTK_WIDGET(gtk_builder_get_object(b, "window"));

    {
        GdkGeometry geo;

        geo.min_width = 300;
        geo.min_height = 300;

        ASSERT(window);
        gtk_window_set_geometry_hints(GTK_WINDOW(window), 0,
                &geo, GDK_HINT_MIN_SIZE);

        // TODO: add the window size setting to user preferences
        // just before exiting the app.
        gtk_window_resize(GTK_WINDOW(window), 1200, 1000);
    }

    {
        GtkPaned *hpaned = GTK_PANED(gtk_builder_get_object(b, "hpaned"));
        gtk_paned_set_position(hpaned, 700);
    }

     AddBlockSelector(
             GTK_WIDGET(gtk_builder_get_object(b,
                     "blockSelectorTree")),
             GTK_ENTRY(gtk_builder_get_object(b,
                     "selectedBlockEntry")));
 
    noteBook = GTK_NOTEBOOK(gtk_builder_get_object(b, "notebook"));

    NewTab(0, 0);

    Connect(b, "window", "destroy", gtk_main_quit, 0);
    Connect(b, "quitMenu", "activate", gtk_main_quit, 0);
    Connect(b, "quitButton", "clicked", gtk_main_quit, 0);
    Connect(b, "newTabButton", "clicked", NewTab, 0);

    // We are done with this builder
    g_object_unref(G_OBJECT(b));
}


static void
catcher(int sig) {

    fprintf(stderr, "Caught signal %d\n"
        "\n"
        "  Consider running:  gdb -pid %u\n\n",
        sig, getpid());

    ASSERT(0);
}

#define DEFAULT_SPEW_LEVEL (5)

int main(int argc, char *argv[]) {

    signal(SIGSEGV, catcher);
    signal(SIGABRT, catcher);

    qsSetSpewLevel(DEFAULT_SPEW_LEVEL);

    gtk_init(&argc, &argv);

    InitCSS();

    setup_widgets();

    gtk_main(); 

    CleanupBlockSelector();
    qsErrorFree();

    DSPEW("exiting");
    return 0;
}
