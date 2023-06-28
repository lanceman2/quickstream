
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkx.h>

#include "../lib/debug.h"

#include "treeView_test.h"


// TODO: This has problems getting the path that can be used by whatever
// will load the plugins.  The path is relative to what? and what
// environment variables where used?
//
// We need to add a path finder function, that verifies/normalizes the
// path.

// Returns name to add the file, or 0 to not.
//
// The returned value must be free()d.
static
char *GetName(int dirfd, const char *path) {

    DASSERT(path);
    char *name = strdup(path);
    ASSERT(name, "strdup() failed");
    return name;
}


// Returns true to add the file, or false to not.
//
// The dirfd should not be closed by this function.
static
bool CheckFile(int dirfd, const char *path) {

    size_t len = strlen(path);
    if(len <= 2) return false;

    if(strcmp(&path[len-2], ".h") == 0) {
        //DSPEW("%s", path);
        return true;
    }
    return false;
}


static
void setup_widgets(const char *path) {

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title (GTK_WINDOW(win), "GTK TREE VIEW");

    g_signal_connect(G_OBJECT(win), "destroy", gtk_main_quit, 0);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    {
        //////////////////////////////////////////////////////
        //   THIS BLOCK IS THE API WE ARE TESTING
        //////////////////////////////////////////////////////
        GtkWidget *tree = treeViewCreate();
        treeViewAdd(tree, path, path, CheckFile, GetName);
        treeViewShow(tree, "Choose Block");
        gtk_container_add(GTK_CONTAINER(vbox), tree);
    }

    gtk_container_add(GTK_CONTAINER(win), vbox);

    gtk_widget_show_all(vbox);
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

    setup_widgets(argv[1]?argv[1]:"..");

    gtk_main(); 

    return 0;
}
