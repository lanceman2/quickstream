#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "../lib/Dictionary.h"
#include "../include/quickstream.h"

#include "quickstreamGUI.h"



static GtkWidget *dialog = 0;
static struct Layout *layout = 0;
static GtkListStore *blockListModel = 0;
static GtkWidget *treeview = 0;


// GTK3 BUG: The "response" signal caller corrupts the user data so we
// have a static struct Layout *layout and do not use the user_data set
// to the layout.
static
void Response_cb(GtkWidget *w, int res, void *user_data) {

    DASSERT(dialog);
    DASSERT(layout);

    switch(res) {

        default:
            DSPEW("res=%d", res);
            gtk_widget_destroy(dialog);
            dialog = 0;
            layout = 0;
            blockListModel = 0;
            treeview = 0;
    }
}


struct BlockIter {

    GtkListStore *model;
    struct Block *block;
    GtkTreeIter *iter;
};


// GTK3 CALLBACK INTERFACE BUG (2022-12-03):
//
// Not reported.  I don't need the extra pain/work.  It's more of an
// opinion.
//
// This function callback prototype is terrible.  You get a "new_text"
// string with no pointer to the selected data that you may be interested
// in.  The thing that causes the "edit" is just unknown to this function.
//
// Luckily we already have a mapping from thread pool name to thread pool
// pointer with qsGraph_getThreadPool().
//
static void
Edited_cb(GtkCellRendererText *cell, const gchar *path_string,
        const gchar *threadPoolName, GtkTreeModel *model) {

    DASSERT(layout);
    DASSERT(layout->qsGraph);

    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_string);
    gtk_tree_model_get_iter(model, &iter, path);

    struct QsThreadPool *tp = qsGraph_getThreadPool(layout->qsGraph,
            threadPoolName);
    ASSERT(tp, "Thread pool named \"%s\" was not found",
            threadPoolName);

    GtkTreeModel *tmodel;

    GtkTreeSelection *sel =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    if(!gtk_tree_selection_get_selected(sel, &tmodel, &iter)) {
        ERROR("Nothing selected");
        // No block row is selected.
        return;
    }
    DASSERT(tmodel == GTK_TREE_MODEL(blockListModel));

    gtk_list_store_set(GTK_LIST_STORE(blockListModel), &iter,
            1, threadPoolName, -1);

    struct QsBlock *b = 0;

    gtk_tree_model_get(GTK_TREE_MODEL(blockListModel), &iter,
            2, &b, -1);

    qsBlock_setThreadPool(b, tp);

    DSPEW("block name =\"%s\"", qsBlock_getName(b));
}


static
GtkWidget *CreateCellEditor(struct Layout *l) {

    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
            GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* Create the list store for showing blocks and there thread pools */

    DASSERT(blockListModel == 0);
    blockListModel = GetBlockListModel(l);

    if(!blockListModel) {
        GtkWidget *w =
            gtk_label_new("But In This Case, There Are No Simple "
                    "Blocks To Configure Thread Pools For.");
        gtk_container_add(GTK_CONTAINER(sw), w);
        return sw;
    }

    gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(blockListModel),
            0, GTK_SORT_ASCENDING);

    GtkTreeModel *threadPoolSelectModel = MakeThreadPoolListStore(l);


    treeview = gtk_tree_view_new_with_model(
            GTK_TREE_MODEL(blockListModel));

    gtk_tree_selection_set_mode(
            gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
                    GTK_SELECTION_SINGLE);


    GtkCellRenderer *renderer;

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "editable", FALSE, NULL);

    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
            -1, "Simple Block", renderer,
            "text", 0/*column*/,
            NULL);


    renderer = gtk_cell_renderer_combo_new();
    g_object_set(renderer,
            "model", threadPoolSelectModel,
            "text-column", 0,
            "has-entry", FALSE,
            "editable", TRUE,
            NULL);
    g_signal_connect(renderer, "edited",
            G_CALLBACK(Edited_cb), threadPoolSelectModel);

    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
            -1, "Thread Pool", renderer,
            "text", 1,
            NULL);

    g_object_unref(blockListModel);
    g_object_unref(threadPoolSelectModel);

    gtk_container_add(GTK_CONTAINER(sw), treeview);

    return sw;
}


static inline
void _CreateWindow(struct Layout *l) {

    DASSERT(l);
    DASSERT(l->window);
    DASSERT(l->window->win);
    DASSERT(l->qsGraph);
    DASSERT(dialog == 0);
    layout = l;

    size_t LEN = 90;
    char title[LEN];
    snprintf(title, LEN, "Assign Block's Thread Pools For Graph \"%s\"",
            qsGraph_getName(l->qsGraph));

    GtkDialogFlags flags =
        GTK_DIALOG_DESTROY_WITH_PARENT |
        GTK_DIALOG_MODAL;
    dialog = gtk_dialog_new_with_buttons(title,
            GTK_WINDOW(l->window->win),
            flags,
            "_Close", GTK_RESPONSE_CLOSE,
            NULL);

    gtk_window_set_transient_for(GTK_WINDOW(dialog),
            GTK_WINDOW(l->window->win));

    // GTK3 BUG (2022-12-03):
    //
    // Calling gtk_dialog_run() brakes the content_area; so we use a
    // response callback and gtk_widget_show_all(dialog).

    g_signal_connect_swapped(dialog, "response",
            G_CALLBACK(Response_cb), l);

    GtkWidget *content_area =
        gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    gtk_box_pack_start(GTK_BOX(vbox),
            gtk_label_new("You can change a simple blocks thread pool on the right"),
            FALSE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), CreateCellEditor(l),
            TRUE/**/, TRUE/**/, 0);

    gtk_box_pack_start(GTK_BOX(content_area), vbox,
            TRUE/**/, TRUE/**/, 0);

    gtk_widget_show_all(dialog);
}


void ShowAssignBlocksToThreadsWindow(struct Layout *l) {

    // Each showing will be destroyed after.
    _CreateWindow(l);
}


void CleanupAssignBlocksToThreadsWindow(void) {

    // Nothing to do.

    //if(!win) return;

    //g_object_unref(win);

    //gtk_widget_destroy(win);
}
