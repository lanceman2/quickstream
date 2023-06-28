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


static
void Response_cb(GtkWidget *w, int res,
               gpointer user_data) {

    DASSERT(w == dialog);
    DASSERT(dialog);

    DSPEW("res=%d", res);

    gtk_widget_destroy(GTK_WIDGET(dialog));
    dialog = 0;
}


// We need to change the window title if the block name changes,
// so this function keeps the presentation of the block name
// consistent.
//
static inline void SetWindowTitle(const struct Block *b) {

    DASSERT(dialog);
    DASSERT(b);
    DASSERT(b->qsBlock);

    const size_t TLEN = 128;
    char winTitle[TLEN];
    snprintf(winTitle, TLEN, "Configure Block \"%s\"",
            qsBlock_getName(b->qsBlock));

    gtk_window_set_title(GTK_WINDOW(dialog), winTitle);
}


static void RenameBlock_cb(GtkWidget *ent, struct Block *b) {

    DASSERT(dialog);
    DASSERT(b);
    DASSERT(b->qsBlock);

    const char *newName = gtk_entry_get_text(GTK_ENTRY(ent));

    const char *name = qsBlock_getName(b->qsBlock);

    if(strcmp(newName, name) == 0) {
        // No name change.
        //
        // Setting the entry text will change the highlighting
        // of the text in the entry:
        gtk_entry_set_text(GTK_ENTRY(ent), name);
        return;
    }

    bool changedName = !qsBlock_rename(b->qsBlock, newName);

    // At this point the qsBlock name is changed or will not change.

    if(!changedName) {
        // GUI user feedback:
        gtk_entry_set_text(GTK_ENTRY(ent), name);
        return;
    }

    // The block name changed.

    // GUI user feedback:
    SetWindowTitle(b);
    // Set the text on the label part of the block widget:
    gtk_label_set_text(GTK_LABEL(b->nameLabel), newName);
    // Reset the block label tooltip text:
    char *tooltipText = Splice2(
                "The name of this instance of this block is: ",
                newName);
    gtk_widget_set_tooltip_text(b->nameLabel, tooltipText);
    free(tooltipText);
}


// TODO: Maybe make it so that the user can interact with 2 dialog windows
// at one time, so that they can copy and paste from one block
// configuration to another.  That would require removing the
// GTK_DIALOG_MODAL flag and adding conditionals (code) in
// CreateBlockConfigWindow().
//
static inline
void CreateBlockConfigWindow(struct Block *b) {

    DASSERT(b->layout);
    DASSERT(b->layout->window);
    DASSERT(b->layout->window->win);
    DASSERT(b->qsBlock);
    DASSERT(dialog == 0);

    GtkDialogFlags flags =
        GTK_DIALOG_DESTROY_WITH_PARENT |
        GTK_DIALOG_MODAL;
    dialog = gtk_dialog_new_with_buttons("TITLE",
            GTK_WINDOW(b->layout->window->win),
            flags,
            "_Close", GTK_RESPONSE_CLOSE,
            NULL);
    SetWindowTitle(b);

    gtk_window_set_transient_for(GTK_WINDOW(dialog),
            GTK_WINDOW(b->layout->window->win));

    // GTK3 BUG (2022-12-03):
    //
    // Calling gtk_dialog_run() brakes the content_area; so we use a
    // response callback and gtk_widget_show_all(dialog).

    g_signal_connect_swapped(dialog, "response",
            G_CALLBACK(Response_cb), dialog);

    GtkWidget *content_area =
        gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,
            5/*spacing*/);

    { // Setting Block Name
      /////////////////////////////////////////////////////////////////
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,
            4/*spacing*/);

        GtkWidget *label = gtk_label_new("Block Name:");
        gtk_widget_set_name(label, "configBlockTitle");
        gtk_box_pack_start(GTK_BOX(hbox),  label,
                FALSE/*expand*/, TRUE/*fill*/,
                3/*padding*/);

        GtkWidget *ent = gtk_entry_new();
        g_signal_connect(ent, "activate",
                G_CALLBACK(RenameBlock_cb), b);
        gtk_widget_set_name(ent, "configBlockTitle");
        gtk_entry_set_text(GTK_ENTRY(ent),
                qsBlock_getName(b->qsBlock));
        gtk_box_pack_start(GTK_BOX(hbox),  ent,
                TRUE/*expand*/, TRUE/*fill*/,
                3/*padding*/);

        //gtk_container_add(GTK_CONTAINER(vbox), hbox);
        gtk_box_pack_start(GTK_BOX(vbox),  hbox,
                FALSE/*expand*/, TRUE/*fill*/,
                3/*padding*/);
    }

    if(BlockHasConfigAttrubutes(b)) {

        ///////////////////////////////////////////////////////////////
        // List of Attributes for Configuration

        GtkWidget *frame = gtk_frame_new("Configure Attributes");
        gtk_widget_set_name(frame, "configure_attributes");

        GtkWidget *grid = gtk_grid_new();
        gtk_widget_set_name(grid, "configure_attributes");
        gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
        gtk_grid_set_row_spacing(GTK_GRID(grid), 4);

        MakeConfigWidgets(b, grid);
        gtk_container_add(GTK_CONTAINER(frame), grid);

        //gtk_container_add(GTK_CONTAINER(vbox), hbox);
        gtk_box_pack_start(GTK_BOX(vbox), frame,
                TRUE/*expand*/, TRUE/*fill*/,
                3/*padding*/);
    }

    gtk_box_pack_start(GTK_BOX(content_area),  vbox,
            TRUE/*expand*/, TRUE/*fill*/,
            3/*padding*/);

    gtk_widget_show_all(dialog);
}


void ShowBlockConfigWindow(struct Block *b) {

    // Each showing will be destroyed after.
    CreateBlockConfigWindow(b);
}


#if 0
void CleanupBlockConfigWindow(void) {

    // Nothing to do.

    //if(!win) return;

    //g_object_unref(win);

    //gtk_widget_destroy(win);
}
#endif
