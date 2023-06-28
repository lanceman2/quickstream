#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "../lib/Dictionary.h"
#include "../include/quickstream.h"

#include "quickstreamGUI.h"


// The menu:
static GtkWidget *menu = 0;

static GtkWidget *openMenuItem;
static GtkWidget *showButtonBarMenuItem;
static GtkWidget *removeSelectedMenuItem;
static GtkWidget *saveMenuItem;

// ...Or at least attempted to be loaded:
static
char *lastBlockLoaded = 0;



// The current Layout using the popup menu
//
// We change this each time we popup the menu.
static struct Layout *layout = 0;



static void CloseTab_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);
    CloseTab(layout);
}

static void NewTab_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);
    ASSERT(CreateTab(layout->window, 0));
}

static void NewWindow_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);
    CreateWindow(0);
}


static void RemoveSelected_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);
    DASSERT(layout->selectedBlocks);

    while(layout->selectedBlocks) {
        DASSERT(layout->selectedBlocks->layout == layout);
        DASSERT(layout->selectedBlocks->layout->layout);
        DASSERT(layout->selectedBlocks->parent);
        gtk_widget_destroy(layout->selectedBlocks->parent);
    }
}


static void ShowButtonBar_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);
    ShowButtonBar(layout, true);
}




static 
char *GetPathFromGUI(void) {

#define O_TITLE "Open Super Block as a Flow Graph"

    GtkWidget *dialog = gtk_file_chooser_dialog_new(O_TITLE,
            0/*parent*/,
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Cancel", GTK_RESPONSE_CANCEL,
	    "_Open", GTK_RESPONSE_ACCEPT,
            /* null terminator */
            NULL);
    ASSERT(dialog);

    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);

    {
        GtkWidget *l = gtk_label_new(O_TITLE);
        gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(chooser), l);

        GtkFileFilter *filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, "*.so");
        gtk_file_filter_add_pattern(filter, "*.so");
        gtk_file_chooser_add_filter(chooser, filter);

        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, "*");
        gtk_file_filter_add_pattern(filter, "*");
        gtk_file_chooser_add_filter(chooser, filter);
    }

    if(lastBlockLoaded)
        gtk_file_chooser_set_filename(chooser, lastBlockLoaded);

    gint res = gtk_dialog_run(GTK_DIALOG(dialog));

    char *filename = 0;

    if(res != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(dialog);
        return 0;
    }

    filename = gtk_file_chooser_get_filename(chooser);

    gtk_widget_destroy(dialog);

    // GTK3 should work.
    ASSERT(filename);
    ASSERT(filename[0]);

    if(lastBlockLoaded) {
#ifdef DEBUG
        memset(lastBlockLoaded, 0, strlen(lastBlockLoaded));
#endif
        free(lastBlockLoaded);
        lastBlockLoaded = strdup(filename);
        ASSERT(lastBlockLoaded, "strdup() failed");
    }

    DSPEW("filename=\"%s\"", filename);

    return filename;
}


static void OpenBlock_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);

    char *path = GetPathFromGUI();

    if(!path) return;

    struct Layout *l = CreateLayout(layout->window, path);

    if(l)
        RecreateTab(layout/*old layout*/, l/*new layout*/);

#ifdef DEBUG
    memset(path, 0, strlen(path));
#endif
    g_free(path);
}


static void AddBlock_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);

    char *path = GetPathFromGUI();

    if(!path) return;

    CreateBlock(layout, path, 0, -3.0, -3.0, 0,
                1 /*0 unselectAllButThis,
                   1 add this block to selected
                   2 no selection change */);
#ifdef DEBUG
    memset(path, 0, strlen(path));
#endif
    g_free(path);
}


static void NewTabOpenBlock_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);

    char *path = GetPathFromGUI();

    if(!path) return;

    CreateTab(layout->window, path);

#ifdef DEBUG
    memset(path, 0, strlen(path));
#endif
    g_free(path);
}


static void BlockThreadConfig_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);
    DASSERT(layout->layout);

    ShowAssignBlocksToThreadsWindow(layout);
    DSPEW();
}


static void ThreadConfig_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);
    DASSERT(layout->layout);

    ShowCreateThreadPools(layout, 0);
    DSPEW();
}

static void DisplayGraphViz_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);
    DASSERT(layout->layout);

    DSPEW("Displaying graph \"%s\" with dot",
            qsGraph_getName(layout->qsGraph));

    // This will display just the current tabs quickstream graph.
    qsGraph_printDotDisplay(layout->qsGraph, false, 0);

    // This would display all existing quickstream graphs.
    //qsGraph_printDotDisplay(0, false, 0);


    DSPEW();
}


// Try to:
//   Save the current tab/graph as a super block, C file, and runner file.
//
static void SaveAsSuperBlock_cb(GtkWidget *w, gpointer data) {

    SaveAs(layout);
    DSPEW();
}


// Try to:
//  Save the current tab/graph as a super block, C file, and runner file.
//
static void SaveSuperBlock_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);
    DASSERT(layout->lastSaveAs);

    Save(layout);
    DSPEW();
}



static GtkWidget *renamePopover = 0;
static GtkWidget *renameEntry = 0;

static void
RenameEntry_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);
    DASSERT(layout->qsGraph);
    DASSERT(layout->layout);
    DASSERT(layout->tabLabel);

    // name string points to internally allocated storage in the widget
    // and must not be freed, modified or stored.
    const char *name = gtk_entry_get_text(GTK_ENTRY(renameEntry));

    // TODO: Check for invalid chars; sanitize name
    //
    // Maybe it's not needed given qsGraph_setName() may do an okay job.
    //
    ERROR("Add code here to sanitize the graph name");


    if(name && name[0] &&
            qsGraph_setName(layout->qsGraph, name) == 0) {
#define T_LEN 64
        char tabLabel[T_LEN];
        snprintf(tabLabel, T_LEN, "Graph \"%s\"",
                qsGraph_getName(layout->qsGraph));
        gtk_label_set_text(GTK_LABEL(layout->tabLabel),
                tabLabel);
    }
    gtk_popover_popdown(GTK_POPOVER(renamePopover));
}


static void RenameTab_cb(GtkWidget *w, gpointer data) {

    DASSERT(layout);
    DASSERT(layout->qsGraph);
    DASSERT(layout->layout);
    DASSERT(layout->tabLabel);

    // Let's try it with a entry in a popover.
    if(!renamePopover) {
        renamePopover = gtk_popover_new(layout->tabLabel);
        ASSERT(renamePopover);
        DASSERT(renameEntry == 0);
        renameEntry = gtk_entry_new();
        gtk_container_add(GTK_CONTAINER(renamePopover), renameEntry);
        gtk_widget_show(renameEntry);
        g_signal_connect(renameEntry, "activate",
                G_CALLBACK(RenameEntry_cb), 0);
    } else {
        gtk_popover_set_relative_to(GTK_POPOVER(renamePopover),
                layout->tabLabel);
    }

    gtk_entry_set_text(GTK_ENTRY(renameEntry),
            qsGraph_getName(layout->qsGraph));
    gtk_popover_popup(GTK_POPOVER(renamePopover));

    DSPEW();
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


static void
AddSeparator(void) {

    DASSERT(menu);
    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_container_add(GTK_CONTAINER(menu), sep);
    gtk_widget_show(sep);
}


static inline
void CreatePopupMenu(void) {

    DASSERT(!menu);

    menu = gtk_menu_new();
    ASSERT(menu);


    MakeMenuItem("New Tab", NewTab_cb);
    MakeMenuItem("New Window", NewWindow_cb);
    AddSeparator();
    openMenuItem = MakeMenuItem("Open Block As Graph ...",
            OpenBlock_cb);
    MakeMenuItem("Add Block ...", AddBlock_cb);
    MakeMenuItem("New Tab From Block ...", NewTabOpenBlock_cb);
    AddSeparator();
    MakeMenuItem("Rename Graph", RenameTab_cb);
    AddSeparator();
    MakeMenuItem("Assign Blocks To Thread Pools ...",
            BlockThreadConfig_cb);
    MakeMenuItem("Manage Thread Pools ...", ThreadConfig_cb);
    AddSeparator();
    MakeMenuItem("Save As ...", SaveAsSuperBlock_cb);
    saveMenuItem = MakeMenuItem("Save", SaveSuperBlock_cb);
    AddSeparator();
    MakeMenuItem("Display GraphViz Dot Graph Image",
            DisplayGraphViz_cb);
    AddSeparator();
    showButtonBarMenuItem = MakeMenuItem("Show Button Bar",
            ShowButtonBar_cb);
    AddSeparator();
    removeSelectedMenuItem = MakeMenuItem("Remove Selected Blocks",
            RemoveSelected_cb);
    MakeMenuItem("Close Tab", CloseTab_cb);

    g_object_ref(menu);
}


void ShowGraphTabPopupMenu(struct Layout *l) {

    DASSERT(l);

    if(!menu)
        CreatePopupMenu();

    layout = l;


    gtk_widget_set_sensitive(openMenuItem, (l->blocks)?FALSE:TRUE);

    gtk_widget_set_sensitive(showButtonBarMenuItem,
            (l->buttonBarShowing)?FALSE:TRUE);

    gtk_widget_set_sensitive(removeSelectedMenuItem,
            (l->selectedBlocks)?TRUE:FALSE);

    gtk_widget_set_sensitive(saveMenuItem,
            (l->lastSaveAs)?TRUE:FALSE);


    gtk_menu_popup_at_pointer(GTK_MENU(menu), 0);
}


void CleanupGraphTabPopupMenu(void) {

    if(!menu) return;

    g_object_unref(menu);

    menu = 0;

    if(lastBlockLoaded) {
#ifdef DEBUG
        memset(lastBlockLoaded, 0, strlen(lastBlockLoaded));
#endif
        free(lastBlockLoaded);
        lastBlockLoaded = 0;
    }
}
