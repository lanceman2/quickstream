#define _GNU_SOURCE
#include <libgen.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "../lib/FindFullPath.h"
#include "../lib/mprintf.h"
#include "../lib/Dictionary.h"
#include "../include/quickstream.h"

#include "qsg_treeView.h"
#include "quickstreamGUI.h"


// This is only one TreeView popup menu per process.
// It seems to work on all windows.

// There are two pop up menus in this file:
//
// 1. If the user clicked on a file (block).
static GtkWidget *pathMenu = 0;
//
// 2. If the user clicked on a directory:
static GtkWidget *dirMenu = 0;


// menu item to load a super block as a graph that is in the block menu.
static GtkWidget *loadFlattenedBlock;
// menu item to view block source code that is in the directory menu.
static GtkWidget *viewSource_menu;

static GtkWidget *viewExample_menu;



// The blockPath depends on what treeView selection was made.
static char *blockPath = 0;
// The dirPath depends on what treeView selection was made.
static char *dirPath = 0;

static char *blockSourcePath = 0;

static char *examplePath = 0;


static GtkEntryBuffer *envBuffer = 0;



static void
ReloadBlocksEnv_finalCB(GtkWidget *w, GtkWidget *win) {

    if(!win) {
        if(envBuffer) {
            g_object_unref(envBuffer);
            envBuffer = 0;
        }
        return;
    }

    DASSERT(envBuffer);

    const char *env = gtk_entry_buffer_get_text(envBuffer);

    if(env && env[0])
        setenv("QS_BLOCK_PATH", env, 1);
    else
        unsetenv("QS_BLOCK_PATH");


    DSPEW("QS_BLOCK_PATH=\"%s\"", env);

    RecreateTreeView(window);

    g_object_unref(envBuffer);
    envBuffer = 0;

    gtk_widget_destroy(win);
}


static void
ReloadBlocksEnv_cancelCB(GtkWidget *w,
        GtkWidget *win) {

    DASSERT(envBuffer);

    g_object_unref(envBuffer);
    envBuffer = 0;

    gtk_widget_destroy(win);
}


static
char *GetStringDialog(char *header, char *text) {

    DASSERT(window);
    DASSERT(window->win);
    DASSERT(!envBuffer);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_default_size(GTK_WINDOW(win), 1000, 100);
    gtk_window_set_title (GTK_WINDOW(win),
            "Edit the environment variable QS_BLOCK_PATH before"
            " reloading the tree view");
    gtk_window_set_transient_for(
            GTK_WINDOW(win), GTK_WINDOW(window->win));
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

    GtkWidget *w = gtk_label_new (NULL);
    gtk_label_set_markup(GTK_LABEL(w), "<b>QS_BLOCK_PATH</b>");
    gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, FALSE, 0);

    envBuffer = gtk_entry_buffer_new(NULL, 0);
    w = gtk_entry_new_with_buffer(envBuffer);
    gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, FALSE, 0);
    char *env = getenv("QS_BLOCK_PATH");
    if(env)
        gtk_entry_buffer_set_text(envBuffer, env, strlen(env));

    g_signal_connect(win, "destroy",
            G_CALLBACK(ReloadBlocksEnv_finalCB), 0);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    {
        GtkWidget *b = gtk_button_new_with_label("Cancel");
        gtk_box_pack_end(GTK_BOX(hbox), b, FALSE, FALSE, 0);
        g_signal_connect(b, "clicked",
            G_CALLBACK(ReloadBlocksEnv_cancelCB), win);

        b = gtk_button_new_with_label("Ok");
        gtk_box_pack_end(GTK_BOX(hbox), b, FALSE, FALSE, 0);
        g_signal_connect(b, "clicked",
            G_CALLBACK(ReloadBlocksEnv_finalCB), win);
    }

    


    gtk_widget_show_all(win);

    return 0;
}


static void ReloadBlocksEnv_cb(GtkWidget *w, gpointer data) {

    DASSERT(window);
    DASSERT(window->treeView);

    GetStringDialog(0, 0);
}


static void ReloadBlocks_cb(GtkWidget *w, gpointer data) {

    DASSERT(window);
    DASSERT(window->treeView);

    RecreateTreeView(window);
}


static void ShowAllBlocks_cb(GtkWidget *w, gpointer data) {

    DASSERT(window);
    DASSERT(window->treeView);

    gtk_tree_view_expand_all(GTK_TREE_VIEW(window->treeView));
}


static void CloseDirViews_cb(GtkWidget *w, gpointer data) {

    DASSERT(window);
    DASSERT(window->treeView);

    // collapse all directories that are not we are not selected.


    GtkTreePath *tpath = GetTreePath(window->treeView);
    // pop up one directory in the tree view thingy.
    //
    // AND SO ON...
    //
ERROR("WRITE MORE CODE HERE  (%p) ++++++++++++++++++++++++++++++++++++++++++++", tpath);
// LETS NOT ASSERT HERE JUST NOW We'll find this ---------------


    //gtk_tree_view_expand_all(GTK_TREE_VIEW(window->treeView));
}



static void OpenTerminal_cb(GtkWidget *w, gpointer data) {

    DASSERT(window);
    DASSERT(window->treeView);

    pid_t pid = fork();

    if(pid <= -1) {
        WARN("fork() failed");
        return;
    }
    if(pid != 0)
        // I am the parent.
        return;

    // I am the child.

    char *run = getenv("QS_TERMINAL");

    if(0 != chdir(dirPath)) {
        WARN("execlp() failed");
        exit(1);
    }

    if(!run)
        // Fallback default terminal to run:
        run = "gnome-terminal";

    // Note: there is no reason to free run because we will call
    // execlp(3).

    // TODO: add an option to change the shell or remove it all together
    // and run the program without a shell to parse the command.
    DSPEW("using bash running: \"%s\" in %s", run, getcwd(0, 0));
    execlp("bash", "/bin/bash", "-c", run, NULL);

    WARN("execlp() failed");
    exit(1);
}



static void Load_cb(GtkWidget *w, gpointer data) {

    DASSERT(blockPath);
    DSPEW("loading block blockPath=\"%s\"", blockPath);
    
    CreateBlock(window->layout, blockPath, 0, -1.0, -1.0, 0, 1);
}


static void LoadGraph_cb(GtkWidget *w, gpointer data) {

    DASSERT(blockPath);
    DSPEW("loading block blockPath as graph=\"%s\"", blockPath);

    struct Layout *l = CreateLayout(window, blockPath);

    if(l)
        RecreateTab(window->layout/*old layout*/, l/*new layout*/);
}


static void ViewSource_cb(GtkWidget *w, gpointer data) {

    DASSERT(blockSourcePath);
    DSPEW("Editing block source=\"%s\"", blockSourcePath);

    pid_t pid = fork();

    if(pid <= -1) {
        WARN("fork() failed");
        return;
    }
    if(pid != 0)
        // I am the parent.
        return;

    // I am the child.

    char *run = getenv("QS_EDITOR");

    if(!run)
        run = "gvim --servername quicksreamGUI_00 --remote";

    run = mprintf("%s %s", run, blockSourcePath);

    // Note: there is no reason to free run because we will call
    // execlp(3).

    // TODO: add an option to change the shell or remove it all together
    // and run the program without a shell to parse the command.
    DSPEW("using bash running: \"%s\" in %s", run, getcwd(0, 0));
    execlp("bash", "/bin/bash", "-c", run, NULL);

    WARN("execlp() failed");
    exit(1);
}


static void ViewExample_cb(GtkWidget *w, gpointer data) {

    DASSERT(blockSourcePath);
    DASSERT(examplePath);
    DSPEW("Viewing ===> \"%s\"", examplePath);

    // We'll need the parent (or grand-parent) PID in the child processes.
    pid_t ppid = getpid();

    // We wish to not make zombie processes and not wait for children, and
    // so we fork() twice and so on ...:

    pid_t pid = fork();

    if(pid <= -1) {
        WARN("fork() failed");
        DASSERT(0);
        return;
    }
    if(pid) {
        // I am the parent.
        int status = -1;
        ASSERT(pid == waitpid(pid, &status, 0));
        DASSERT(WEXITSTATUS(status) == 16);
        INFO("waitpid() got child status of %d", WEXITSTATUS(status));
        return;
    }

    // I'm the child.

    pid = fork();

    if(pid <= -1) {
        WARN("fork() failed");
        DASSERT(0);
        return;
    }

    if(pid) {
        // I am the second parent.
        //
        // Remove zombie children like by calling signal(SIGCHLD, SIG_IGN)
        // which we could not do after the first fork() because calling
        // signal(SIGCHLD, SIG_IGN) in that process broke other things.
        // And since this is a parent if this exits there will be no
        // zombie from its' child if this parent exits soon.  No need to
        // do this:
        //ASSERT(signal(SIGCHLD, SIG_IGN) != SIG_ERR);

        // It's important to call _exit(0) and not exit(0) which does too
        // many things that we do not want.
        _exit(16);
    }

    // I am the child that will launch the program of interest.

    // TODO: Linux specific use of the /proc/ file system
    // to find the this program that we are running.
    //
    // Can this be any easier?  I think not.  Just run
    // "/proc/PPID/exe".

    char *run = mprintf("/proc/%u/exe", ppid);

    // Note: there is no reason to free run because we will call
    // execlp(3).

    DSPEW("Running: \"%s %s\" in %s", run, examplePath, getcwd(0, 0));
    execlp(run, "quickstreamGUI", examplePath, NULL);

    WARN("execlp() failed");
    exit(33);
}




static inline
GtkWidget *MakeMenuItem(GtkWidget *menu, const char *text,
        void *callback) {

    DASSERT(menu);

    GtkWidget *mi = gtk_menu_item_new_with_label(text);
    gtk_container_add(GTK_CONTAINER(menu), mi);
    g_signal_connect(mi, "activate", G_CALLBACK(callback), 0);
    gtk_widget_show_all(mi);
    return mi;
}


static inline
void AddSeparator(GtkWidget *menu) {
    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_container_add(GTK_CONTAINER(menu), sep);
    gtk_widget_show(sep);
}


static inline
void CreatePathPopupMenu(void) {

    pathMenu = gtk_menu_new();
    ASSERT(pathMenu);
    DASSERT(window);
    DASSERT(window->layout);
    DASSERT(window->layout->window == window);

    MakeMenuItem(pathMenu, "Load Into Graph", Load_cb);

    AddSeparator(pathMenu);
    loadFlattenedBlock = MakeMenuItem(pathMenu,
            "Load As Graph", LoadGraph_cb);

    AddSeparator(pathMenu);
    viewSource_menu =
        MakeMenuItem(pathMenu,
                "View Source in Text Editor ...", ViewSource_cb);
    viewExample_menu =
        MakeMenuItem(pathMenu,
                "View Graph Example in Editor ...", ViewExample_cb);



    g_object_ref(pathMenu);
}


static inline void FreeDirPath(void) {

    if(dirPath) {
#ifdef DEBUG
        memset(dirPath, 0, strlen(dirPath));
#endif
        free(dirPath);
        dirPath = 0;
    }
}

static inline void FreeBlockPath(void) {

    if(blockPath) {
#ifdef DEBUG
        memset(blockPath, 0, strlen(blockPath));
#endif
        free(blockPath);
        blockPath = 0;
    }
}

static inline void FreeBlockSourcePath(void) {

    if(blockSourcePath) {
#ifdef DEBUG
        memset(blockSourcePath, 0, strlen(blockSourcePath));
#endif
        free(blockSourcePath);
        blockSourcePath = 0;
    }
}

static inline void FreeExamplePath(void) {

    if(examplePath) {
#ifdef DEBUG
        memset(examplePath, 0, strlen(examplePath));
#endif
        free(examplePath);
        examplePath = 0;
    }
}



static void CreateDirPopupMenu(void) {

    dirMenu = gtk_menu_new();
    ASSERT(dirMenu);

    DASSERT(window);
    DASSERT(window->layout);
    DASSERT(window->layout->window == window);

    MakeMenuItem(dirMenu, "Reload Tree View", ReloadBlocks_cb);
    AddSeparator(dirMenu);
    MakeMenuItem(dirMenu, "Reload Tree View With Env ...",
            ReloadBlocksEnv_cb);
    AddSeparator(dirMenu);
    MakeMenuItem(dirMenu, "Show All Blocks", ShowAllBlocks_cb);
    MakeMenuItem(dirMenu, "Close Some Directory Views", CloseDirViews_cb);
     AddSeparator(dirMenu);
    MakeMenuItem(dirMenu, "Open a Terminal in this Directory ...",
            OpenTerminal_cb);

    g_object_ref(dirMenu);
}



static inline void ShowDirPopupMenu(GtkTreeView *treeView) {

    DASSERT(dirPath);
    DASSERT(dirPath[0]);
    DASSERT(treeView);

    if(!dirMenu)
        CreateDirPopupMenu();
    DASSERT(dirMenu);

    gtk_menu_popup_at_pointer(GTK_MENU(dirMenu), 0);
}


// newBlockPath is malloc()ed memory, and we must free() it some time.  We
// get lazy about free()ing it, which is okay in this case.
//
// If newBlockPath is not set this is a click on a directory in the tree
// thingy, and so dir will be set.  If dir is set is must be free()ed.
//
void ShowTreeViewPopupMenu(GtkTreeView *treeView,
        char *newBlockPath, char *dir) {

    DASSERT(treeView);
    DASSERT(newBlockPath || dir);
    DASSERT(!newBlockPath || !dir);

    DASSERT(window);
    DASSERT(window->layout);

    // Lazy clean-up of old values:
    FreeBlockPath();
    FreeBlockSourcePath();
    FreeDirPath();
    FreeExamplePath();


    if(!newBlockPath) {
        DASSERT(dir);
        dirPath = dir;
        ShowDirPopupMenu(treeView);
        return;
    }

    if(!pathMenu)
        CreatePathPopupMenu();

    blockPath = newBlockPath;

    if(blockPath) {
        // TODO: Linux specific code: ".so" library suffix.
        // TODO: Linux specific code: "/" directory string.
        //
        // Strip off ".so"
        size_t l = strlen(blockPath);
        char *p = 0;
        if(l > 3 && strcmp(blockPath + l - 3, ".so") == 0) {
            p = malloc(l - 2);
            ASSERT(p, " strdup() failed");
            strncpy(p, blockPath, l - 3);
            p[l - 3] = '\0';
        } else {
            p = strdup(blockPath);
            ASSERT(p, " strdup() failed");
        }
        examplePath = mprintf("%s/qs_examples/%s.so",
                dirname(p), basename(p));

        // We try two different functions to see if the example block
        // exists.  realpath(3) and FindFullPath() from libquickstream.so.
        //
        // The returned value from realpath() must be free()ed, if it was
        // set.
        char *rp = realpath(examplePath, 0);

        if(!rp)
            // Function used to find/load blocks in libquickstream.so.
            //
            // The returned value from FindFullpath() must be free()ed,
            // if it was set.
            rp = FindFullPath(examplePath, qsBlockDir/*prefix*/, ".so",
                    getenv("QS_BLOCK_PATH"));

        // So much stupid dummy string memory:
        free(p);
        free(examplePath);

        // If set rp is it:
        examplePath = rp;
        DSPEW("examplePath=\"%s\"", examplePath);
        // If set examplePath will get free()ed later.
    }

    if(window->layout->blocks)
        gtk_widget_set_sensitive(loadFlattenedBlock, FALSE);
    else
        gtk_widget_set_sensitive(loadFlattenedBlock, TRUE);

    blockSourcePath = GetBlockSourcePath(treeView);

    if(blockSourcePath)
        gtk_widget_set_sensitive(viewSource_menu, TRUE);
    else
        gtk_widget_set_sensitive(viewSource_menu, FALSE);

    if(examplePath)
        gtk_widget_set_sensitive(viewExample_menu, TRUE);
    else
        gtk_widget_set_sensitive(viewExample_menu, FALSE);


    gtk_menu_popup_at_pointer(GTK_MENU(pathMenu), 0);
}


void CleanupTreeViewPopupMenu(void) {

    FreeBlockPath();
    FreeBlockSourcePath();
    FreeDirPath();
    FreeExamplePath();

    if(pathMenu) {
        g_object_unref(pathMenu);
        pathMenu = 0;
    }

    if(dirMenu) {
        g_object_unref(dirMenu);
        dirMenu = 0;
    }
}
