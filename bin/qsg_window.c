#include <stdio.h>
#include <stdint.h>
#include <gtk/gtk.h>

#include "../include/quickstream.h"

#include "../lib/debug.h"
#include "../lib/Dictionary.h"


#include "quickstreamGUI.h"
#include "qsg_treeView.h"


#define NOTEBOOK_GROUP_NAME  "quickstream"


static uint32_t numWindows = 0;

// We need only one of these since we only use it when the window is
// just make.
static gulong configureHandlerID = 0;


// The last top level window to have focus:
struct Window *window = 0;

static GtkAccelGroup *accelGroup = 0;



static void DestroyWindow_cb(struct Window *w) {

    // Looks like this is called before the gtk_window is destroyed.

    DASSERT(w);

#ifdef DEBUG
    memset(w, 0, sizeof(*w));
#endif
    free(w);

    if(--numWindows == 0)
        gtk_main_quit();
}


static gboolean
ChangeTab_cb(GtkNotebook *notebook,
                GtkWidget *page, guint page_num,
                struct Window *w) {
    DASSERT(w);
    DASSERT(page);

    struct Layout *l = g_object_get_data(G_OBJECT(page), "Layout");
    ASSERT(l);

    // Set the current tab layout.
    w->layout = l;

    DASSERT(l->qsGraph);

    DSPEW("Set layout to graph \"%s\"",
            qsGraph_getName(l->qsGraph));

    return FALSE;
}


static gboolean
Focus_cb(GtkWidget *win, GdkEventFocus *e, struct Window *w) {

    DASSERT(w);
    DASSERT(win);
    DASSERT(w->win == win);

    window = w;

    return FALSE;
}


gboolean Configure_cb(GtkWidget *win, GdkEventConfigure *e, struct Window *w) {

    // TODO: Set the paned position so that the tree view looks good.
    // Maybe somehow remember the paned setting.

    DASSERT(w);
    DASSERT(win);
    DASSERT(w->win == win);
    DASSERT(w->paned);
    DASSERT(configureHandlerID);

    g_signal_handler_disconnect(win, configureHandlerID);
    configureHandlerID = 0;

    GdkRectangle rec;
    gtk_widget_get_allocation(win, &rec);

    int panedX = rec.width - 330;
    if(rec.width < 330*2)
        panedX = rec.width/2;
    // The default before this call was too small.  You could not read any
    // of the block file names in the tree view.
    gtk_paned_set_position(GTK_PANED(w->paned), panedX);

    DSPEW();
    return TRUE;
}


void CreateWindow(const char *path) {

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    DASSERT(win);
    gtk_widget_set_events(win,
            gtk_widget_get_events(win) |
            GDK_STRUCTURE_MASK);

    if(!accelGroup)
        accelGroup = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(win), accelGroup);

    // TODO: set a good window title and have it change as it runs.
    gtk_window_set_title(GTK_WINDOW(win), "quickstream");

    gtk_container_set_border_width(GTK_CONTAINER(win), 0);

    // TODO: this setting is somewhat arbitrary;
    gtk_window_set_default_size(GTK_WINDOW(win), 1560, 900);


    struct Window *w = calloc(1, sizeof(*w));
    ASSERT(w, "calloc(1,%zu) failed", sizeof(*w));
    w->win = win;

    gtk_widget_set_events(win,
            gtk_widget_get_events(win)|
            GDK_FOCUS_CHANGE_MASK);
    g_signal_connect(win, "focus-in-event",
            G_CALLBACK(Focus_cb), w);

    ASSERT(g_object_get_data(G_OBJECT(win), "Window") == 0);
    g_object_set_data_full(G_OBJECT(win), "Window", w,
            (void (*) (void *))DestroyWindow_cb);
    ASSERT(g_object_get_data(G_OBJECT(win), "Window") == w);
    ++numWindows;

    {
        GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_paned_set_wide_handle(GTK_PANED(paned), TRUE);
        {
            GtkWidget *notebook = gtk_notebook_new();
            ASSERT(notebook);
            w->notebook = GTK_NOTEBOOK(notebook);
            gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
            gtk_notebook_set_group_name(GTK_NOTEBOOK(notebook),
                    NOTEBOOK_GROUP_NAME);
            g_signal_connect(notebook, "switch-page",
                    G_CALLBACK(ChangeTab_cb), w);
            if(!path || !CreateTab(w, path))
                // Try without a path.
                CreateTab(w, 0);
            gtk_paned_pack1(GTK_PANED(paned), notebook, TRUE, TRUE);
        }
        {
            GtkWidget *sw = gtk_scrolled_window_new (NULL, NULL);
            gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
                        GTK_SHADOW_ETCHED_IN);
            gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

            CreateTreeView(GTK_CONTAINER(sw), w);
            gtk_paned_pack2(GTK_PANED(paned), sw, FALSE/*resize*/, TRUE);
        }

        gtk_container_add(GTK_CONTAINER(win), paned);

        //gtk_paned_set_position(GTK_PANED(paned), 50);

        w->paned = GTK_PANED(paned);
        DASSERT(configureHandlerID == 0);
        configureHandlerID = g_signal_connect(win, "configure-event",
                G_CALLBACK(Configure_cb), w);
    }

    gtk_widget_show_all(win);
}
