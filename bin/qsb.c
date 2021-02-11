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


#define CREATE_BLOCK_BUTTON   (1) // 1 = left mouse


static gboolean WorkArea_buttonReleaseCB(GtkLayout *layout,
        GdkEventButton *e, void *data) {

    if(e->type != GDK_BUTTON_RELEASE) {
        // This should not happen; but I have seen stupider shit.
        ASSERT(0, "Did not get GDK_BUTTON_RELEASE event");
        return FALSE; // FALSE = go to next widget
    }
    if(e->button != CREATE_BLOCK_BUTTON)
        return FALSE; // FALSE = go to next widget

    const char *blockFile = GetSelectedBlockFile();
    if(!blockFile)
        return TRUE; // Eat this event at this widget


    AddBlock(layout, blockFile, e->x, e->y);

    return TRUE; // Eat this event at this widget
}


static inline void MakeWorkArea(void) {

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

    g_signal_connect(GTK_WIDGET(layout), "button-release-event",
            G_CALLBACK(WorkArea_buttonReleaseCB), 0/*userData*/);

    // We are done with this builder
    g_object_unref(G_OBJECT(b));
}


static gboolean NewTab(GtkWidget *w, gpointer data) {

    MakeWorkArea();
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
