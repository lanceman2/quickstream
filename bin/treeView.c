
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


static GtkWidget *createTree(void) {

    enum {
        TITLE_COLUMN,
        AUTHOR_COLUMN,
        CHECKED_COLUMN,
        N_COLUMNS
    };

    GtkTreeStore *store =
        gtk_tree_store_new(N_COLUMNS, /* Total number of columns */
            G_TYPE_STRING,   /* Book title              */
            G_TYPE_STRING,   /* Author                  */
            G_TYPE_BOOLEAN); /* Is checked out?         */

    GtkTreeIter iter;

    gtk_tree_store_append(store, &iter, NULL); /* Acquire an iterator */

    gtk_tree_store_set(store, &iter,
                    TITLE_COLUMN, "The Principle of Reason",
                    AUTHOR_COLUMN, "Martin Heidegger",
                    CHECKED_COLUMN, FALSE,
                    -1);


    GtkTreeIter iter1;  /* Parent iter */
    GtkTreeIter iter2;  /* Child iter  */

    /* Acquire a top-level iterator */
    gtk_tree_store_append(store, &iter1, NULL);
    gtk_tree_store_set(store, &iter1,
                    TITLE_COLUMN, "The Art of Computer Programming",
                    AUTHOR_COLUMN, "Donald E. Knuth",
                    CHECKED_COLUMN, FALSE,
                    -1);

    /* Acquire a child iterator */
    gtk_tree_store_append(store, &iter2, &iter1);
    gtk_tree_store_set (store, &iter2,
                    TITLE_COLUMN, "Volume 1: Fundamental Algorithms",
                    -1);

    gtk_tree_store_append(store, &iter2, &iter1);
    gtk_tree_store_set(store, &iter2,
                    TITLE_COLUMN, "Volume 2: Seminumerical Algorithms",
                    -1);

    gtk_tree_store_append(store, &iter2, &iter1);
    gtk_tree_store_set(store, &iter2,
                    TITLE_COLUMN, "Volume 3: Sorting and Searching",
                    -1);


    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));


    GtkCellRenderer *trenderer = gtk_cell_renderer_text_new();


    GtkTreeViewColumn *column =
        gtk_tree_view_column_new_with_attributes ("Author",
                                                   trenderer,
                                                   "text", AUTHOR_COLUMN,
                                                   NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW(tree), column);


    column =
        gtk_tree_view_column_new_with_attributes ("Title",
                                                   trenderer,
                                                   "text", TITLE_COLUMN,
                                                   NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);


    GtkCellRenderer *crenderer = gtk_cell_renderer_toggle_new();


    column =
        gtk_tree_view_column_new_with_attributes ("Checked out",
                                                   crenderer,
                                                   "active", CHECKED_COLUMN,
                                                   NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);


    gtk_widget_show(tree);

    return tree;
}


static
void setup_widgets(void) {

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title (GTK_WINDOW(win), "GTK TREE VIEW");

    g_signal_connect(G_OBJECT(win), "destroy", gtk_main_quit, 0);


    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_container_add(GTK_CONTAINER(vbox), createTree());

    gtk_container_add(GTK_CONTAINER(win), vbox);

    gtk_widget_show(vbox);
    gtk_widget_show(win);
}


static void
catcher(int sig) {

    fprintf(stderr, "Caught signal %d\n"
        "\n"
        "  Consider running:  gdb -pid %u\n\n",
        sig, getpid());

    while(1) usleep(1000);
}


int main(int argc, char *argv[]) {

    signal(SIGSEGV, catcher);
    signal(SIGABRT, catcher);

    gtk_init(&argc, &argv);

    setup_widgets();

    gtk_main(); 

    return 0;
}
