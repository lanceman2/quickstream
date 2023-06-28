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
GtkTreeModel *model = 0;
static uint32_t numThreadPools = 0;
static GtkWidget *treeview = 0;


enum {
    NEW_THREADPOOL = 2,
    REMOVE_THREADPOOL
};


#define DEFAULT_MAXTHREADS  3


static inline
struct QsThreadPool *GetSelectedThreadPool(GtkTreeIter *iter) {

    DASSERT(treeview);

    GtkTreeModel *tmodel;
    GtkTreeSelection *sel =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    if(!gtk_tree_selection_get_selected(sel, &tmodel, iter))
        // No thread pool is selected.
        return 0;

    // I do not understand why GTK3 is forcing me to get the
    // model when I already have it.  Yes, I needed to get the
    // iter too.  Can a tree view have more than one model?
    DASSERT(tmodel == model);

    struct QsThreadPool *tp = 0;
    // Get the thread pool that is selected:
    gtk_tree_model_get(model, iter, 2/*column*/, &tp, -1);

    DASSERT(tp);

    return tp;
}


static
void Response_cb(GtkWidget *w, int res,
        void *data) {

    DASSERT(dialog);
    DASSERT(model);
    DASSERT(layout);
    DASSERT(layout->qsGraph);

    DSPEW("res=%d", res);

    switch(res) {

        case NEW_THREADPOOL:
        {
            struct QsThreadPool *tp = qsGraph_createThreadPool(
                    layout->qsGraph, DEFAULT_MAXTHREADS, 0);
            ASSERT(tp);

            GtkTreeIter iter;
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                    0, qsThreadPool_getName(tp),
                    1, DEFAULT_MAXTHREADS,
                    2, tp, // This one is not rendered to the GUI.
            -1);
            ++numThreadPools;
        }
            break;

        case REMOVE_THREADPOOL:

        {
            DASSERT(numThreadPools);
            GtkTreeIter iter;

            if(numThreadPools == 1) {
                fprintf(stderr,
                        "We can not remove the last thread pool.\n");
                // TODO: It would be better to disable the
                // "Remove Thread Pool" button.
                break;
            }

            struct QsThreadPool *tp = GetSelectedThreadPool(&iter);

            if(!tp) {
                fprintf(stderr, "No thread pool is selected");
                return;
            }

            // Sometimes this gtk_list_store_remove() call returns FALSE
            // but succeeds.  Not how it is documented.  I'll just
            // ignore the return value and assume it works.  It does not
            // make sense that it would fail given we got the iter from
            // a valid selection above.
            //
            // Yet another GTK3 BUG.
            //
            gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
            qsThreadPool_destroy(tp);
            --numThreadPools;
        }
            break;

        default:
        {
            // Here we destroy the dialog window.

            DASSERT(numThreadPools);

            GtkTreeIter iter;

            struct QsThreadPool *tp = GetSelectedThreadPool(&iter);
            if(tp)
                // Make it so that when simple blocks are loaded they
                // will use this thread pool by default.
                qsGraph_setDefaultThreadPool(layout->qsGraph, tp);

            gtk_widget_destroy(GTK_WIDGET(dialog));
            dialog = 0;
            layout = 0;
            model = 0;
            treeview = 0;
            numThreadPools = 0;
        }
    }
}


// We need to change the window title if the block name changes,
// so this function keeps the presentation of the block name
// consistent.
//
static inline void SetWindowTitle(const struct Layout *l) {

    DASSERT(dialog);
    DASSERT(l);
    DASSERT(l->qsGraph);

    const size_t TLEN = 128;
    char winTitle[TLEN];
    snprintf(winTitle, TLEN, "Manage Thread Pools For Graph \"%s\"",
            qsGraph_getName(l->qsGraph));

    gtk_window_set_title(GTK_WINDOW(dialog), winTitle);
}


static void EditName_cb(GtkCellRendererText *cell,
        const gchar *path_string,
        const gchar *new_text,
        GtkListStore *model) {

    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_string);
    gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);

    struct QsThreadPool *tp = 0;
    gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 2/*column*/, &tp, -1);

    if(qsThreadPool_setName(tp, new_text)) {
        fprintf(stderr, "Failed to set a thread pool to name \'%s\"\n",
                new_text);
        // Failed to set name of thread pool.
        return;
    }

    gtk_list_store_set(model, &iter,
            // We just change the thread pool name.
            0, new_text,
            -1);
}

    
static void EditMaxThreads_cb(GtkCellRendererText *cell,
        const gchar *path_string,
        const gchar *new_text,
        GtkTreeModel *model) {

    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_string);
    gtk_tree_model_get_iter(model, &iter, path);

    char *end = 0;
    uint32_t maxThreads = strtoul(new_text, &end, 10);
    if(end == new_text || maxThreads == 0) {
        fprintf(stderr, "Bad max threads \"%s\"\n", new_text);
        return;
    }

    struct QsThreadPool *tp = 0;
    gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 2/*column*/, &tp, -1);
    DASSERT(tp);

    qsThreadPool_setMaxThreads(tp, maxThreads);

    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
            // Just change column 1 the max threads.
            1/*column*/, maxThreads, -1);
}


static inline
void _CreateWindow(struct Layout *l, GtkWindow *parent) {

    DASSERT(l);
    DASSERT(l->window);
    DASSERT(l->window->win);
    DASSERT(dialog == 0);
    layout = l;

    if(!parent)
        parent = GTK_WINDOW(l->window->win);

    GtkDialogFlags flags =
        GTK_DIALOG_DESTROY_WITH_PARENT |
        GTK_DIALOG_MODAL;
    dialog = gtk_dialog_new_with_buttons("TITLE", parent,
            flags,
            "_New Thread Pool", NEW_THREADPOOL,
            "_Remove Thread Pool", REMOVE_THREADPOOL,
            "_Close", GTK_RESPONSE_CLOSE,
            NULL);
    SetWindowTitle(l);

    gtk_window_set_transient_for(GTK_WINDOW(dialog),
            parent);

    // GTK3 BUG (2022-12-03):
    //
    // Not reported.  I don't need the extra pain/work.  Ya, they will
    // just say it's not a bug and you cannot do that.  Fuck it.
    //
    // Calling gtk_dialog_run() brakes the content_area; so we use a
    // response callback and gtk_widget_show_all(dialog).

    GtkWidget *content_area =
        gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    {
        GtkWidget *label = gtk_label_new(
                "Add, Remove, and Configure Thread Pools");
        gtk_box_pack_start(GTK_BOX(content_area), label,
                FALSE/*expand*/, TRUE/*fill*/,
                3/*padding*/);
    }

    GtkWidget *sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (sw),
            GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
            GTK_POLICY_AUTOMATIC,
            GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(content_area), sw, TRUE, TRUE, 0);


    model = MakeThreadPoolsEditStore(l, &numThreadPools);

    gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(model),
            0, GTK_SORT_ASCENDING);


    DASSERT(numThreadPools);

    g_signal_connect_swapped(dialog, "response",
            G_CALLBACK(Response_cb), 0);

    treeview = gtk_tree_view_new_with_model(model);

    /////////////////////////////////////////////////
    // Column 0: thread pool name
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
            "editable", TRUE, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
            -1, "Thread Pool Name", renderer,
            "text", 0,
            NULL);
    g_signal_connect(renderer, "edited",
            G_CALLBACK(EditName_cb), model);

    /////////////////////////////////////////////////
    // Column 1: thread pool maximum threads
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
            "editable", TRUE, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
            -1, "Maximum Threads", renderer,
            "text", 1,
            NULL);
    g_signal_connect(renderer, "edited",
            G_CALLBACK(EditMaxThreads_cb), model);

    /////////////////////////////////////////////////
    // Column 2: thread pool pointer is not rendered


    gtk_container_add(GTK_CONTAINER(sw), treeview);

    g_object_unref(model);


    gtk_widget_show_all(dialog);
}


void ShowCreateThreadPools(struct Layout *l, GtkWindow *parent) {

    // Each showing will be destroyed after.
    _CreateWindow(l, parent);
}
