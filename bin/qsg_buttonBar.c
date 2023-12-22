#include <gtk/gtk.h>

#include "../include/quickstream.h"

#include "../lib/debug.h"

#include "quickstreamGUI.h"



static char *GetFilename(struct Layout *layout) {

    DASSERT(layout);
    DASSERT(layout->layout);
    DASSERT(layout->qsGraph);

#define TLEN  128
    char title[TLEN];
    snprintf(title, TLEN, "Save Graph \"%s\" as a Super Block",
            qsGraph_getName(layout->qsGraph));

    GtkWidget *dialog = gtk_file_chooser_dialog_new(title, 0/*parent*/,
                    GTK_FILE_CHOOSER_ACTION_SAVE,
                    "_Cancel", GTK_RESPONSE_CANCEL,
		    "_Save", GTK_RESPONSE_ACCEPT,
                    /* null terminator */
                    NULL);

    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

    {
        // Setup the default save as filename as something related
        // to the name of the quickstream graph.
        char *name = (void *) qsGraph_getName(layout->qsGraph);
        DASSERT(name);
        DASSERT(name[0]);
        const size_t len = strlen(name);
        char *s = malloc(len + 4);
        ASSERT(s, "strdup() failed");
        strcpy(s, name);
        name = s;
        s = name + len - 1;
        while(s != name && *s != '.') --s;
        if(s == name) {
            // s == name
            s = name + len;
            *s = '.';
        }
        *(++s) = 's';
        *(++s) = 'o';
        *(++s) = '\0';

        gtk_file_chooser_set_current_name(chooser, name);
#ifdef DEBUG
        memset(name, 0, len);
#endif
        free(name);
    }

    if(layout->lastSaveAs) {
        // Should be like filename.so, with ".so" suffix.
        char *s = layout->lastSaveAs;
        size_t len = strlen(layout->lastSaveAs);
        // This string has 3 chars extra so we can add a ".so"
        // suffix.
        *(s + len) = '.';
        *(s + len + 1) = 's';
        *(s + len + 2) = 'o';
        *(s + len + 3) = '\0';
        gtk_file_chooser_set_filename(chooser, layout->lastSaveAs);
        // Remove the ".so" suffix.
        *(s + len) = '\0';
    }

    {
        snprintf(title, TLEN, "Save Graph \"%s\" as a Super Block:"
                " Creates FILE.c, FILE.so, and FILE",
                qsGraph_getName(layout->qsGraph));

        GtkWidget *l = gtk_label_new(title);
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

    //gtk_file_chooser_set_filename(chooser, existing_filename);

    gint res = gtk_dialog_run(GTK_DIALOG(dialog));

    if(res != GTK_RESPONSE_ACCEPT) {

        gtk_widget_destroy(dialog);
        return 0; // nothing to do
    }

    // We have a request and we try to save the graph as a super block.

    char *filename = gtk_file_chooser_get_filename(chooser);
    ASSERT(filename);
    ASSERT(filename[0]);

    gtk_widget_destroy(dialog);

    {
        // Make filename a little larger.
        size_t len = strlen(filename) + 4; // more chars to play with.
        char *s = calloc(1, len);
        ASSERT(s, "calloc(1,%zu) failed", len);
        strcpy(s, filename);
#ifdef DEBUG
        memset(filename, 0, strlen(filename));
#endif
        g_free(filename);
        filename = s;
    }

    size_t len = strlen(filename);

    if(len > 2 && filename[len-1] == 'c' && filename[len-2] == '.')
        // Strip off the ".c" from the end.
        filename[len-2] = '\0';
    if(len > 3 && strcmp(filename + len - 3, ".so") == 0)
        // Strip off the ".so" from the end.
        filename[len-3] = '\0';

    // Now filename is the "name" in the 3 files: name.c , name.so, name.

    return filename;
}


// filename_in must be malloced memory that is without ".so", as in
// filename.so => filename.   It also must have 4 chars more at
// it's end.
//
// The ownership of filename_in allocated memory is passed to this
// function.
//
// Will transfer the filename memory to l->lastSaveAs (as filename.so) if
// successful.
//
static
void _Save(struct Layout *l, char *filename_in) {

    DASSERT(l);

    char *filename;
    if(filename_in)
        filename = filename_in;
    else {
        DASSERT(l->lastSaveAs);
        filename = l->lastSaveAs;
    }

    DASSERT(filename);
    DASSERT(filename[0]);

    // Now we try to save the graph to files, but before we can:
    //
    //   - add this app's metadata to the graph

    struct QsGraph *g = l->qsGraph;
    DASSERT(g);

    qsGraph_clearMetaData(g);


    // TODO: Writing serialized binary data like this may not be portable
    // code.

    // We need to serialize some data to put in the file we are saving.
    // The qsGraph_setMetaData() stores the data that is written to the
    // file as a base64 string in the block C code file.  We just need
    // it to be a single chunk of memory at this point.

    size_t mSize = 0;
    uint32_t numBlocks = 0;

    // 1. First the names of the blocks as they will be loaded in the
    //    graph.  Just getting the size for now.
    for(struct Block *b = l->blocks; b; b = b->next) {
        DASSERT(b->qsBlock);
        size_t l = strlen(qsBlock_getName(b->qsBlock));
        DASSERT(l);
        mSize += l + 1;
        ++numBlocks;
    }
    // Add a null terminator.
    mSize += 1;

    uint32_t stringPadding = 0;
    if(mSize % 8) {
        // Pad it to the nearest 64 = (8x8) bits, because memory access at
        // ??? boundaries can get broken.
        mSize += (stringPadding = (mSize % 8));
    }

    struct GUILayout *gl;

    // 2. Next the block positions and orientation on the layout.
    //    Just the size.
    mSize += numBlocks * sizeof(*gl);

    // Now we have a size.
    char *mem = calloc(1, mSize);
    ASSERT(mem, "calloc(1,%zu) failed", mSize);
    char *s = mem;

    // 1. First the names of the blocks as they will be loaded in the
    //    graph.
    for(struct Block *b = l->blocks; b; b = b->next)
        s += sprintf(s, "%s", qsBlock_getName(b->qsBlock)) + 1;
    // Add a null terminator.
    ++s;

    if(stringPadding)
        // Pad it to the nearest word size, because memory access at
        // non-word boundaries can get broken. Or can it??
        s += stringPadding;

    // 2. Next the block positions and orientation on the layout.
    gl = (struct GUILayout *) s;
    for(struct Block *b = l->blocks; b; b = b->next) {
        gl->x = b->x;
        gl->y = b->y;
        gl->orientation = GetOrientation(b);
        ++gl;
    }

    qsGraph_setMetaData(g, "quickstreamGUI", mem, mSize);

#ifdef DEBUG
    memset(mem, 0, mSize);
#endif
    free(mem);

    {
        // Remove the 3 files that we will be writing, if we can.
        //
        // Ya, the GetFilename() added 4 chars to filename.
        //
        size_t len = strlen(filename);
        filename[len] = '.';
        filename[len+1] = 's';
        filename[len+2] = 'o';
        filename[len+3] = '\0';
        // Remove file filename.so
        unlink(filename);
        //
        filename[len+1] = 'c';
        filename[len+2] = '\0';
        // Remove file filename.c
        unlink(filename);
        //
        // Back to filename
        filename[len] = '\0';
#ifdef DEBUG
        filename[len+1] = '\0';
#endif
        // Remove file filename
        unlink(filename);
    }

    // Write the 3 files:
    //
    // This may or may not succeed.
    //
    // TODO: We could add a little '*' to the window title as a visual
    // flag, like in vim.
    //
    if(qsGraph_save(g, filename, 0, 0) == 0) {
        fprintf(stderr, "Successfully Saved quickstream graph as "
                "%s.c %s.so and %s\n",
                filename, filename, filename);

        if(!l->lastSaveAs) {
            // We have the first successful save as for this
            // graph/layout.
            DASSERT(filename_in);
            gtk_widget_set_sensitive(l->saveButton, TRUE);
        }

        if(l->lastSaveAs && filename_in) {
            // We have a new "Save As" file name.
            // Free the old one.
#ifdef DEBUG
            memset(l->lastSaveAs, 0, strlen(l->lastSaveAs));
#endif
            free(l->lastSaveAs);
        }
        if(filename_in)
            // Become the owner of the filename_in memory.
            l->lastSaveAs = filename_in;

        // TODO: Looks like GTK3 tooltips are broken for toolkits
        // on widgets that are in overlays.  Googling shows that
        // this has been a problem for many years.  The tooltip
        // we setup here now will never be seen.
        //
        // Set the Save button's tooltip text with this filename.
        const char *format =
            "Save this graph as files: %s.c %s.so and %s";
        size_t ln = 3*strlen(filename) + strlen(format) - 5;
        char *tooltipText = calloc(1, ln);
        ASSERT(tooltipText, "calloc(1,%zu) failed", ln);
        snprintf(tooltipText, ln, format, filename, filename, filename);
        gtk_widget_set_tooltip_text(l->saveButton, tooltipText);
        //DSPEW("Set Save tooltip to: %s", tooltipText);
#ifdef DEBUG
        memset(tooltipText, 0, ln);
#endif
        free(tooltipText);

    } else if(filename_in) {
        // Failed to save.
        //
        // We do not save this string memory for this case.

#ifdef DEBUG
        memset(filename_in, 0, strlen(filename_in));
#endif
        free(filename_in);
    }

    //DSPEW("l->lastSaveAs=\"%s\"", l->lastSaveAs);
}


static
void SaveAs_cb(GtkToggleButton *button, struct Layout *l) {

    char *filename = GetFilename(l);

    if(!filename) return;

    _Save(l, filename);

    //DSPEW();
}


void SaveAs(struct Layout *l) {

    SaveAs_cb(0, l);
}



void Save(struct Layout *l) {

    DASSERT(l);
    DASSERT(l->lastSaveAs);
    DASSERT(l->lastSaveAs[0]);
    //DSPEW("l->lastSaveAs=\"%s\"", l->lastSaveAs);

    _Save(l, 0);
}


static
void Save_cb(GtkToggleButton *button, struct Layout *l) {

    Save(l);
}




static
void Halt_cb(GtkToggleButton *button, struct Layout *l) {

    DASSERT(l);
    DASSERT(l->qsGraph);

    bool isActive = gtk_toggle_button_get_active(button);

    if(l->isHalted && isActive) return;
    if(!l->isHalted && !isActive) return;

    if(isActive)
        qsGraph_halt(l->qsGraph);
    else
        qsGraph_unhalt(l->qsGraph);
}

static
void Run_cb(GtkToggleButton *button, struct Layout *l) {

    DASSERT(l);
    DASSERT(l->qsGraph);

    bool isActive = gtk_toggle_button_get_active(button);

    if(l->isRunning && isActive) return;
    if(!l->isRunning && !isActive) return;

    if(isActive)
        qsGraph_start(l->qsGraph);
    else
        qsGraph_stop(l->qsGraph);
}

static
void Hide_cb(GtkButton *button, struct Layout *l) {

    DASSERT(l);
    DASSERT(l->qsGraph);
    ShowButtonBar(l, false);
}


// A gtk_check_button is a gtk_toggle_button.
static inline GtkToggleButton *
AddCheckButton(struct Layout *l, GtkBox *hbox, const char *text,
        void *callback) {

    DASSERT(l);
    DASSERT(hbox);

    GtkWidget *b = gtk_check_button_new_with_mnemonic(text);
    gtk_widget_set_name(b, "buttonBar");
    g_signal_connect(b, "toggled", G_CALLBACK(callback), l);
    gtk_box_pack_start(GTK_BOX(hbox), b, FALSE, FALSE, 1);
    return GTK_TOGGLE_BUTTON(b);
}

// A gtk_check_button is a gtk_toggle_button.
static inline GtkWidget *
AddButton(struct Layout *l, GtkBox *hbox, const char *text,
        void *callback) {

    DASSERT(l);
    DASSERT(hbox);

    GtkWidget *b = gtk_button_new_with_mnemonic(text);
    // TODO: Looks like setting button CSS is broken, at least
    // for buttons on a widget that is on a overlay.
    gtk_widget_set_name(b, "buttonBar");
    g_signal_connect(b, "clicked", G_CALLBACK(callback), l);
    gtk_box_pack_start(GTK_BOX(hbox), b, FALSE, FALSE, 1);
    return b;
}


static
void Destroy(GtkWidget *object, struct Layout *l) {

    DASSERT(l);
    DSPEW();
    // We needed to let this file know when the layout and this buttonBar
    // are destroyed, for SetHaltButton() and SetRunButton().
    l->buttonBar = 0;
}


// BUG (TODO): The button bar button tooltips will not display.  May be
// something to do with the button bar being in a GTK overlay.  Maybe
// we'll figure out how to make it work someday; so I leave this unused
// code here for the future if GTK3 gets fixed.  It's most likely GTK3
// developers will just say: "We don't support that."  It's more likely
// am just a dumb-ass.
#if 0
static inline void
SetTooltip(GtkWidget *w, const char *text) {
    gtk_widget_set_has_tooltip(w, TRUE);
    gtk_widget_set_tooltip_text(w, text);
    //gtk_widget_set_has_tooltip(w, TRUE);
}
#else
// Here's a way to leave all those TEXT strings out of the compiled
// binary, and not have the compiler complain about W being an unused
// variable; all while leaving the code in there that should have
// worked.
#  define SetTooltip(W,TEXT) ASSERT(W)
#endif


static
void CreateButtonBar(struct Layout *l) {

    DASSERT(l);
    DASSERT(!l->lastSaveAs);
    GtkOverlay *o = GTK_OVERLAY(l->overlay);
    DASSERT(o);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
    gtk_widget_set_name(hbox, "buttonBox");
    g_signal_connect(hbox, "destroy", G_CALLBACK(Destroy), l);
    gtk_overlay_add_overlay(o, hbox);
    gtk_overlay_set_overlay_pass_through(o, hbox, TRUE);
    gtk_widget_set_halign(hbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(hbox, GTK_ALIGN_START);

    l->haltButton = AddCheckButton(l, GTK_BOX(hbox), "_Halt", Halt_cb);
    SetTooltip(GTK_WIDGET(l->haltButton),
            "(un)halt all the thread pools");

    l->runButton = AddCheckButton(l, GTK_BOX(hbox), "_Run", Run_cb);
    SetTooltip(GTK_WIDGET(l->runButton), "run (or stop) the stream");

    GtkWidget *b = AddButton(l, GTK_BOX(hbox), "_Save As ...", SaveAs_cb);
    SetTooltip(b, "save graph as a super block to selected files");
 
    l->saveButton = AddButton(l, GTK_BOX(hbox), "_Save", Save_cb);
    gtk_widget_set_sensitive(l->saveButton, FALSE);
    SetTooltip(l->saveButton,
            "save graph as a super block to the same files");

    b = AddButton(l, GTK_BOX(hbox), "Hi_de", Hide_cb);
    SetTooltip(b, "hide this button-bar");

    l->buttonBar = hbox;

    gtk_widget_show_all(hbox);
}


// graph halt Feedback 
void SetHaltButton(struct Layout *l, bool isHalted) {

    DASSERT(l);
    if(!l->buttonBar) return;

    DASSERT(l->haltButton);

    bool isActive = gtk_toggle_button_get_active(l->haltButton);
    l->isHalted = isHalted;

    if(isHalted && isActive) return;
    if(!isHalted && !isActive) return;

    gtk_toggle_button_set_active(l->haltButton, isHalted);

    DSPEW("isHalted=%d", isHalted);
}


// graph start/stop stream Feedback 
void SetRunButton(struct Layout *l, bool isRunning) {

    DASSERT(l);
    if(!l->buttonBar) return;

    DASSERT(l->runButton);

    bool isActive = gtk_toggle_button_get_active(l->runButton);
    l->isRunning = isRunning;


    if(isRunning && isActive) return;
    if(!isRunning && !isActive) return;

    gtk_toggle_button_set_active(l->runButton, isRunning);
}



void ShowButtonBar(struct Layout *l, bool doShow) {

    DASSERT(l);
    if(!l->buttonBar)
        CreateButtonBar(l);
    DASSERT(l->buttonBar);

    if(doShow) {
        gtk_widget_show(l->buttonBar);
        l->buttonBarShowing = true;
    } else {
        gtk_widget_hide(l->buttonBar);
        l->buttonBarShowing = false;
    }
}
