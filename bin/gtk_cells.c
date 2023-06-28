/*

gcc -Wall -g -Werror $(pkg-config --cflags gtk+-3.0) gtk_cells.c $(pkg-config --libs gtk+-3.0) -o gtk_cells

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"


static GtkWidget *window = 0;


// An array that maps block to thread pool
// we will change what threadPool points to
// with this dialog widget.
//
struct BlockThreadPool {
    char *blockName;
    const char *threadPool;
} *blockThreadPools = 0;

#define NUM_BLOCKS  6
#define NUM_THREADPOOLS  10


static char *threadPools[NUM_THREADPOOLS];


enum
{
  COLUMN_ITEM_BLOCK,
  COLUMN_ITEM_THREADPOOL,
  NUM_ITEM_COLUMNS
};



static void
add_items(void) {

    blockThreadPools = calloc(NUM_BLOCKS, sizeof(*blockThreadPools));
    ASSERT(blockThreadPools, "calloc(%d, %zu)", NUM_BLOCKS, sizeof(*blockThreadPools));

    for(int i=0; i<NUM_BLOCKS; ++i) {

        const size_t LEN = 64;
        char str[LEN];
        snprintf(str, LEN, "Block %d", i);
        blockThreadPools[i].blockName = strdup(str);
        ASSERT(blockThreadPools[i].blockName, "strdup() failed");
        blockThreadPools[i].threadPool = threadPools[(i + 4)%NUM_THREADPOOLS];
    }
}

static GtkTreeModel *
create_items_model (void)
{
  gint i = 0;
  GtkListStore *model;
  GtkTreeIter iter;

  /* create array */
  add_items();

  /* create list store */
  model = gtk_list_store_new (NUM_ITEM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

  /* add items */
  for (i = 0; i < NUM_BLOCKS; i++)
    {
      gtk_list_store_append(model, &iter);

      gtk_list_store_set(model, &iter,
                          COLUMN_ITEM_BLOCK,
                          blockThreadPools[i].blockName,
                          COLUMN_ITEM_THREADPOOL,
                          blockThreadPools[i].threadPool,
                          -1);
    }

  return GTK_TREE_MODEL (model);
}

static GtkTreeModel *
create_numbers_model (void)
{
  gint i = 0;
  GtkListStore *model;
  GtkTreeIter iter;

  /* create list store */
  model = gtk_list_store_new (1, G_TYPE_STRING);

  /* add numbers */
  for(i = 0; i<NUM_THREADPOOLS; i++)
    {
      const size_t LEN = 64;
      char str[LEN];
    snprintf(str, LEN, "Thread Pool %d", i);

    threadPools[i] = strdup(str);
    ASSERT(threadPools[i], "strdup() failed");

      gtk_list_store_append (model, &iter);

      gtk_list_store_set (model, &iter,
                          0, threadPools[i],
                          -1);
    }

  return GTK_TREE_MODEL (model);
}

static void
cell_edited (GtkCellRendererText *cell,
             const gchar         *path_string,
             const gchar         *new_text,
             GtkTreeModel *model)
{
  GtkTreePath *path = gtk_tree_path_new_from_string(path_string);
  GtkTreeIter iter;

  gtk_tree_model_get_iter (model, &iter, path);

      {
        gint i;


        i = gtk_tree_path_get_indices(path)[0];
        // i is the block number, index into blockThreadPools[]

fprintf(stderr, "i=%d  new_text=%s  path_string=%s\n", i, new_text, path_string);

        blockThreadPools[i].threadPool = new_text;

        gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1,
                            blockThreadPools[i].threadPool, -1);
      }

  gtk_tree_path_free (path);
}

static void
add_columns (GtkTreeView  *treeview,
             GtkTreeModel *items_model,
             GtkTreeModel *numbers_model)
{
  GtkCellRenderer *renderer;

  /* product column */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "editable", FALSE,
                NULL);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                                               -1, "Simple Block", renderer,
                                               "text", COLUMN_ITEM_BLOCK,
                                               NULL);

  /* number column */
  renderer = gtk_cell_renderer_combo_new ();
  g_object_set (renderer,
                "model", numbers_model,
                "text-column", 0,
                "has-entry", FALSE,
                "editable", TRUE,
                NULL);
  g_signal_connect (renderer, "edited",
                    G_CALLBACK (cell_edited), items_model);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                                               -1, "Thread Pool", renderer,
                                               "text", COLUMN_ITEM_THREADPOOL,
                                               NULL);
}

static
void
do_editable_cells (GtkWidget *do_widget)
{

  if (!window)
    {
      GtkWidget *vbox;
      GtkWidget *sw;
      GtkWidget *treeview;
      GtkTreeModel *items_model;
      GtkTreeModel *numbers_model;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "Assign Simple Blocks Thread Pool");
      gtk_container_set_border_width (GTK_CONTAINER (window), 5);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      gtk_box_pack_start (GTK_BOX (vbox),
                          gtk_label_new ("You can change a simple blocks thread pool on the right"),
                          FALSE, FALSE, 0);

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                           GTK_SHADOW_ETCHED_IN);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

      /* create models */
      numbers_model = create_numbers_model (); // combo thingy
      items_model = create_items_model (); // The block and thread pool tree/list.

      /* create tree view */
      treeview = gtk_tree_view_new_with_model (items_model);
      gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
                                   GTK_SELECTION_SINGLE);

      add_columns (GTK_TREE_VIEW (treeview), items_model, numbers_model);

      g_object_unref (numbers_model);
      g_object_unref (items_model);

      gtk_container_add (GTK_CONTAINER (sw), treeview);


      gtk_window_set_default_size (GTK_WINDOW (window), 320, 200);
    }

    gtk_widget_show_all (window);
}


static void
Catcher(int sig) {

    fprintf(stderr, "Caught signal %d\n", sig);
    while(1)
        usleep(10000000);
}


int main(void) {

    signal(SIGSEGV, Catcher);
    signal(SIGABRT, Catcher);

    gtk_init(0, 0);

    do_editable_cells(0);
    
    gtk_main();

    return 0;
}
