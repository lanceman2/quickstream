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
static GtkNotebook *noteBook = 0;
static int tabCreateCount = 0;
static GtkWidget *window = 0;

struct Block *movingBlock = 0;
// The position of the mouse pointer relative to the block at the time of
// the mouse press event, plus the layout position.
static double xi, yi;

// List of cursors with images:
// https://developer.gnome.org/gdk3/stable/gdk3-Cursors.html#GdkCursorType
static GdkCursor *moveCursor = 0;


static inline void SetCursor(GtkWidget *w, GdkCursor *cursor) {
    gdk_window_set_cursor(gtk_widget_get_window(w), cursor);
}


static inline GdkCursor *GetCursor(const char *type) {
    return gdk_cursor_new_from_name(gdk_display_get_default(), type);
}


static void GetWidgetRootXY(GtkWidget *w, double *x, double *y) {

    gint ix, iy;
    gdk_window_get_origin(gtk_widget_get_window(w), &ix, &iy);
    *x = ix;
    *y = iy;
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



static gboolean WorkArea_buttonReleaseCB(GtkLayout *layout,
        GdkEventButton *e, struct Page *page) {

    if(e->type != GDK_BUTTON_RELEASE) {
        // This should not happen.
        ASSERT(0, "Did not get GDK_BUTTON_RELEASE event");
        return FALSE; // FALSE = go to next widget
    }
    if(e->button != CREATE_BLOCK_BUTTON)
        return FALSE; // FALSE = go to next widget

    if(!movingBlock) return TRUE;

    movingBlock = 0;
    SetCursor(GTK_WIDGET(layout), 0);

    return TRUE; // Eat this event at this widget
}


static gboolean WorkArea_mouseMotionCB(GtkLayout *layout,
        GdkEventButton *e, struct Page *page) {

    if(e->type != GDK_MOTION_NOTIFY) {
        // This should not happen.
        ASSERT(0, "Did not get GDK_MOTION_NOTIFY event");
        return FALSE; // FALSE = go to next widget
    }

    if(!movingBlock) return TRUE;

    gtk_layout_move(layout, movingBlock->ebox,
            e->x_root - xi, e->y_root - yi);

    return TRUE; // Eat this event at this widget
}


static gboolean WorkArea_buttonPressCB(GtkLayout *layout,
        GdkEventButton *e, struct Page *page) {

    if(e->type != GDK_BUTTON_PRESS) {
        // This should not happen; but it does.
        // ya. GTK sucks.
        //ASSERT(0, "Did not get GDK_BUTTON_PRESS event");
        return FALSE; // FALSE = go to next widget
    }
    if(e->button != CREATE_BLOCK_BUTTON)
        return FALSE; // FALSE = go to next widget

    const char *blockFile = GetSelectedBlockFile();

    if(!blockFile && !movingBlock) {
        if(!(e->state & GDK_SHIFT_MASK))
            UnselectAllBlocks(page);
        return TRUE; // Eat this event at this widget
    }

    if(!movingBlock) {
        movingBlock = AddBlock(page, layout, blockFile,
                e->x - (xi = 2*MIN_BLOCK_LEN),
                e->y - (yi = 2*MIN_BLOCK_LEN));
        if(!(e->state & GDK_SHIFT_MASK))
            UnselectAllBlocks(page);
        if(!movingBlock)
            return TRUE; // eat event
    } else {

        if(!(e->state & GDK_SHIFT_MASK))
            UnselectAllBlocks(page);

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

    gint rt = gtk_notebook_append_page(noteBook, GTK_WIDGET(layout),
            gtk_label_new(tagName));
    ASSERT(rt >= 0);

    g_signal_connect(GTK_WIDGET(layout), "button-press-event",
            G_CALLBACK(WorkArea_buttonPressCB), page/*userData*/);

    g_signal_connect(GTK_WIDGET(layout), "motion-notify-event",
            G_CALLBACK(WorkArea_mouseMotionCB), page/*userData*/);

    g_signal_connect(GTK_WIDGET(layout), "button-release-event",
            G_CALLBACK(WorkArea_buttonReleaseCB), page/*userData*/);



    // We are done with this builder
    g_object_unref(G_OBJECT(b));
}


static gboolean NewTab(GtkWidget *w, gpointer data) {

    struct Page *page = calloc(1, sizeof(*page));
    ASSERT(page, "calloc(1,%zu) failed", sizeof(*page));
    // TODO: free(page) somewhere.

    page->selectedBlocks = g_tree_new(BlockSelectCompareCB);

    MakeWorkArea(page);
    return TRUE;
}


static inline void
Connect(GtkBuilder *builder, const char *id, const char *action,
        void *callback, void *userData) {
    g_signal_connect(gtk_builder_get_object(builder, id),
            action, G_CALLBACK(callback), userData);
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
