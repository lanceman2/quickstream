#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "../include/quickstream.h"

#include "quickstreamGUI.h"


// The menu:
static GtkWidget *menu = 0;

static GtkWidget *openBlockItemMenu;
static GtkWidget *openBlockWinItemMenu; 

// The current block using the popup menu
static struct Block *block = 0;


static void RemoveBlock_cb(GtkWidget *w, gpointer data) {

    DASSERT(block);
    DASSERT(block->layout);
    DASSERT(block->layout->layout);

    gtk_widget_destroy(block->parent);

    block = 0;
}


static void ConfigureBlock_cb(GtkWidget *w, gpointer data) {

    DASSERT(block);
    DASSERT(block->layout);
    DASSERT(block->layout->layout);

    ShowBlockConfigWindow(block);
}


static inline GtkWidget *MakeMenuItem(
        const char *text, void *callback) {

    DASSERT(menu);

    GtkWidget *mi = gtk_menu_item_new_with_label(text);
    gtk_container_add(GTK_CONTAINER(menu), mi);
    g_signal_connect(mi, "activate", G_CALLBACK(callback), 0);
    gtk_widget_show_all(mi);
    return mi;
}


// In a new tab.
static void OpenBlock_cb(GtkWidget *w, gpointer data) {

    DASSERT(block);
    DASSERT(block->layout);
    DASSERT(block->layout->layout);
    DASSERT(block->layout->window);

    CreateTab(block->layout->window, GetBlockFileName(block));
}

// In a new window
static void OpenBlockWin_cb(GtkWidget *w, gpointer data) {

    DASSERT(block);
    DASSERT(block->layout);
    DASSERT(block->layout->layout);
    DASSERT(block->layout->window);

    CreateWindow(GetBlockFileName(block));
}


static inline
void AddSeparator(void) {

    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_container_add(GTK_CONTAINER(menu), sep);
    gtk_widget_show(sep);
}


static void Done_cb(GtkWidget *m, gpointer data) {

    DASSERT(m);
    DASSERT(m == menu);

    if(block) {
        PopUpBlock(block);
        block = 0;
    }
}


static inline
void CreateBlockPopupMenu(void) {

    DASSERT(!menu);

    menu = gtk_menu_new();
    ASSERT(menu);
    g_signal_connect(menu, "selection-done", G_CALLBACK(Done_cb), 0);

    MakeMenuItem("Remove block", RemoveBlock_cb);
    AddSeparator();
    MakeMenuItem("Configure Block ...", ConfigureBlock_cb);
    AddSeparator();
    openBlockItemMenu = MakeMenuItem("Open Block In New Tab", OpenBlock_cb);
    openBlockWinItemMenu = MakeMenuItem("Open Block In New Window", OpenBlockWin_cb);

    g_object_ref(menu);
}


void ShowBlockPopupMenu(struct Block *b) {

    DASSERT(b);

    if(!menu)
        CreateBlockPopupMenu();

    gtk_widget_set_sensitive(openBlockItemMenu, IsSuperBlock(b));
    gtk_widget_set_sensitive(openBlockWinItemMenu, IsSuperBlock(b));

    block = b;

    gtk_menu_popup_at_pointer(GTK_MENU(menu), 0);
}


void CleanupBlockPopupMenu(void) {

    if(!menu) return;

    g_object_unref(menu);

    //gtk_widget_destroy(blockPopupMenu.menu);

    menu = 0;
}
