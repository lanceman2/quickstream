#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "../lib/Dictionary.h"
#include "../include/quickstream.h"

#include "quickstreamGUI.h"


// This is only one TreeView popup menu per process.
// It seems to work on all windows.

// There are two pop up menus in this file:
//
// 1. If the user clicked on a file.
static GtkWidget *pathMenu = 0;
//
// 2. If the user clicked on a directory:
static GtkWidget *dirMenu = 0;


// menu item to load a super block as a graph
static GtkWidget *loadFlattenedBlock;


// The blockPath depends on what treeView selection was made.
static char *blockPath = 0;
// The dirPath depends on what treeView selection was made.
static char *dirPath = 0;


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


#if 0
static void OpenTerminal_cb(GtkWidget *w, gpointer data) {

    DASSERT(window);
    DASSERT(window->treeView);
ERROR("More code here.");

}
#endif



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


static void

CreateDirPopupMenu(void) {

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
#if 0
    AddSeparator(dirMenu);
    {
ERROR("path=\"%s\"", path);
        char *menuStr = path;
        menuStr += strlen(path);
        // TODO: Linux specific '/' directory separator.
        while(menuStr != path && *menuStr != '/') ++menuStr;
        menuStr = mprintf("Open Terminal Shell in Directory: %s ...", menuStr);
        MakeMenuItem(dirMenu, menuStr, OpenTerminal_cb);
        free(menuStr);
    }
#endif

    g_object_ref(dirMenu);
}



static inline void
ShowDirPopupMenu(void) {

    if(!dirMenu)
        CreateDirPopupMenu();

    gtk_menu_popup_at_pointer(GTK_MENU(dirMenu), 0);
}


// newBlockPath is malloc()ed memory, and we must free() it some time.  We
// get lazy about free()ing it, which is okay in this case.
//
// If newBlockPath is not set this is a click on a directory in the tree
// thingy, and so dir will be set and dir if set must be free()ed.
//
void ShowTreeViewPopupMenu(char *newBlockPath, char *dir) {

    DASSERT(window);
    DASSERT(window->layout);


    if(!newBlockPath) {
        DASSERT(dir);
        dirPath = dir;
ERROR("dir=\"%s\"", dir);
        ShowDirPopupMenu();
        FreeDirPath();
        return;
    }

    if(!pathMenu)
        CreatePathPopupMenu();

    // Lazy free.  Freeing it just before we get a new one to replace it.
    FreeBlockPath();
    //
    blockPath = newBlockPath;

    if(window->layout->blocks)
        gtk_widget_set_sensitive(loadFlattenedBlock, FALSE);
    else
        gtk_widget_set_sensitive(loadFlattenedBlock, TRUE);

    gtk_menu_popup_at_pointer(GTK_MENU(pathMenu), 0);
}


void CleanupTreeViewPopupMenu(void) {

    FreeBlockPath();
    FreeDirPath();

    if(pathMenu) {
        g_object_unref(pathMenu);
        pathMenu = 0;
    }

    if(dirMenu) {
        g_object_unref(dirMenu);
        dirMenu = 0;
    }
}
