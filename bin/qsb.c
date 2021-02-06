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
         * I expect this is a moving target, and this will break in the
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
            // www3 CSS.  We can probably count on this breaking in new
            // versions of GTK3 or maybe not.
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

            "#block > #getter,\n"
            "#block > #setter,\n"
            "#block > #constant,\n"
            "#block > #input,\n"
            "#block > #output {\n"
                "background-color: rgba(147,176,223,0.5);\n"
                "border: 1px solid rgb(118,162,247);\n"
                "font-size: 130%;\n"
                "}\n"
            "#selectedBlock > #getter,\n"
            "#selectedBlock > #setter,\n"
            "#selectedBlock > #constant,\n"
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
        gtk_window_set_geometry_hints(GTK_WINDOW(window), 0, &geo, GDK_HINT_MIN_SIZE);

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

    CSS();

    setup_widgets();

    gtk_main(); 

    CleanupBlockSelector();
    qsErrorFree();

    DSPEW("exiting");
    return 0;
}
