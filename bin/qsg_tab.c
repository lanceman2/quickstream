#include <stdio.h>
#include <stdint.h>
#include <gtk/gtk.h>

#include "../include/quickstream.h"

#include "../lib/debug.h"
#include "../lib/Dictionary.h"

#include "quickstreamGUI.h"



void CloseTab(struct Layout *l) {

    DASSERT(l);
    DASSERT(l->layout);
    DASSERT(l->window);
    DASSERT(l->window->notebook);
    DASSERT(l->window->win);
    DASSERT(l->overlay);

    // We need a copy of the Window pointer since we are about to destroy
    // the layout.
    struct Window *w = l->window;

    // This will destroy the layout notebook page.
    //
    // overlay is the parent container for the layout in the
    // notebook page.  This will also cause struct Layout, l, to be
    // free()ed.
    gtk_widget_destroy(l->overlay);

    if(gtk_notebook_get_n_pages(w->notebook) == 0)
        // There are no more pages so we do not need the window.
        gtk_widget_destroy(w->win);

    DSPEW();
}


static
void CloseTab_cb(GtkWidget *button, struct Layout *l) {

    CloseTab(l);
}


static
gboolean TabLabelButtonPress_cb(GtkWidget *label,
        GdkEventButton *e, struct Layout *l) {

    DASSERT(l);
    DASSERT(l->window);
    DASSERT(l->window->layout);

    DSPEW();

    if(e->type != GDK_BUTTON_PRESS)
        return FALSE;

    if(l->window->layout != l)
        // The tab clicked does not contain the current layout.
        return FALSE; // FALSE => pass event on.

    if(e->button == 3/*3 == right mouse*/) {
        ShowGraphTabPopupMenu(l);
        return TRUE;
    }

    return FALSE;
}


static inline
GtkWidget *CreateTabLabel(struct Layout *l) {

    // Setup the tab label and tab close button
    GtkWidget *bbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *e = gtk_event_box_new();
    gtk_widget_set_events(e, gtk_widget_get_events(e)|
                GDK_BUTTON_PRESS_MASK);
    g_signal_connect(e, "button-press-event",
            G_CALLBACK(TabLabelButtonPress_cb), l);

    const size_t Len = 64;
    char title[Len];
    snprintf(title, Len, "Graph \"%s\"", qsGraph_getName(l->qsGraph));
    GtkWidget *label = gtk_label_new(title);
    gtk_container_add(GTK_CONTAINER(e), label);
    gtk_widget_set_name(label, "tab_label");
    l->tabLabel = label;

    GtkWidget *b = gtk_button_new_from_icon_name("window-close",
                GTK_ICON_SIZE_MENU);
    g_signal_connect(b, "clicked", G_CALLBACK(CloseTab_cb), l);
    gtk_widget_set_name(b, "tab_close");
    gtk_container_set_border_width(GTK_CONTAINER(b), 0);

    gtk_box_pack_start(GTK_BOX(bbox), e,
                TRUE/*expand*/, TRUE/*fill*/, 3/*padding*/);
    gtk_box_pack_start(GTK_BOX(bbox), b,
                FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);
    gtk_widget_show_all(bbox);

    return bbox;
}


struct Layout *RecreateTab(struct Layout *old, struct Layout *l) {

    DASSERT(old);
    DASSERT(l);
    struct Window *w = old->window;
    DASSERT(w);
    DASSERT(l->window == w);

    gint pageNum = gtk_notebook_page_num(w->notebook, old->overlay);
    gtk_notebook_append_page(w->notebook, l->overlay,
            CreateTabLabel(l));

    // This will cleanup the old Layout and it's quickstream Graph.
    gtk_widget_destroy(old->overlay);

    gtk_notebook_reorder_child(w->notebook, l->overlay, pageNum);
    gtk_notebook_set_current_page(w->notebook, pageNum);

    return 0;
}


struct Layout *CreateTab(struct Window *w, const char *path) {

    DASSERT(w);

    struct Layout *l = CreateLayout(w, path);
    if(!l)
        // This should only fail if path is non-zero.
        // Fail
        return 0;

    GtkWidget *tab_label = CreateTabLabel(l);

    gtk_notebook_append_page(w->notebook, l->overlay, tab_label);
    gtk_notebook_set_current_page(w->notebook,
            gtk_notebook_get_n_pages(w->notebook)-1);

    return l;
}
