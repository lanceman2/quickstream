#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "../include/quickstream.h"

#include "quickstreamGUI.h"



// We only make one of all of these:

// The menu:
GtkWidget *menu = 0;

// The current terminal using the menu
struct Terminal *terminal;

// If the pointer is on a port with connections this is set to that port.
struct Port *port;

// Menu item for removing connections:
GtkWidget *removeConnections;

// Menu item for setting setter values:
GtkWidget *setValue;

GtkWidget *makeAlias;
GtkWidget *renameAlias;
GtkWidget *removeAlias;


// The menu items that corresponds to the side (position) to move
// the terminal to.  One menu item will be deactivated.
GtkWidget *left, *right, *top, *bottom;


static GtkWidget *setValuePopover = 0;
static GtkWidget *setValueLabel = 0;
static GtkWidget *setValueEntry = 0;


static inline bool IsNotNumber(char c) {

    if(c >= '0' && c <= '9') return false;
    if(c == '+' || c == '-') return false;
    return true;
}


static inline bool IsNotBoolChar(char c) {

    DASSERT(c);

    if(c != 't' && c != 'f' &&
        c != 'T' && c != 'F' &&
        c != 'y' && c != 'n' &&
        c != 'Y' && c != 'N')
        return true;

    return false;
}

static inline bool GetBool(char c) {
    if(c == 't' || c == 'T' || c == 'y' || c == 'Y')
        return true;
    return false;
}


static void
SetValueEntry_cb(GtkWidget *w, gpointer data) {

    DASSERT(port);
    DASSERT(port->terminal == terminal);
    DASSERT(port->terminal->type == Set);

    struct QsParameter *p = (void *) port->qsPort;
    DASSERT(p);

    const char *text = gtk_entry_get_text(GTK_ENTRY(setValueEntry));

    gtk_popover_popdown(GTK_POPOVER(setValuePopover));

    size_t size = qsParameter_getSize(p);
    DASSERT(size);
    void *value = malloc(size);
    ASSERT(value, "malloc(%zu) failed", size);


    switch(qsParameter_getValueType(p)) {

        case QsValueType_double:
        {
            DASSERT(size % sizeof(double) == 0);
            size_t n = size/sizeof(double);
            const char *s = text;
            double *val = value;
            for(size_t i = 0; i < n;) {
                char *end = 0;
                *val = strtod(s, &end);
                if(end == s)
                    goto finish;
                s = end + 1; // skip to next char
                while(*s && (IsNotNumber(*s) && *s != 'e' && *s != 'E'))
                    ++s;
                ++val;
                ++i;
            }
            qsParameter_setValue(p, value);
        }
            break;
        case QsValueType_uint64:
        {
            DASSERT(size % sizeof(uint64_t) == 0);
            size_t n = size/sizeof(uint64_t);
            const char *s = text;
            uint64_t *val = value;
            for(size_t i = 0; i < n;) {
                char *end = 0;
                *val = strtoull(s, &end, 10);
                if(end == s)
                    goto finish;
                s = end + 1; // skip to next char
                while(*s && IsNotNumber(*s))
                    ++s;
                ++val;
                ++i;
            }
            qsParameter_setValue(p, value);
        }
            break;
        case QsValueType_bool:
        {
            DASSERT(size % sizeof(bool) == 0);
            size_t n = size/sizeof(bool);
            const char *s = text;
            bool *val = value;
            size_t i = 0;
            for(; i < n && *s;) {
                *val = GetBool(*s);
                ++s;
                while(*s && IsNotBoolChar(*s))
                    ++s;
                ++val;
                ++i;
            }
            if(i < n)
                goto finish;

            qsParameter_setValue(p, value);
        }
            break;
        case QsValueType_string32:
        {
            // TODO; We do not support arrays of string32 yet.
            // Changes in ../lib/parameter.c needed too which
            // will ASSERT() if the array length > 1.
            //
            DASSERT(size == 32);

            strncpy(value, text, size-1);
            ((char *) value) [size -1] = '\0';
            qsParameter_setValue(p, value);
        }
            break;

        default:
            //QsTypeNew
            ASSERT(0, "MORE CODE HERE");
            break;
    }

finish:

#ifdef DEBUG
    memset(value, 0, size);
#endif
    free(value);
}


// We can only interact with one popover at a time.
//
// TODO: Remove the other popover, setValuePopover.
// Refactor this C file a little.
//
static GtkWidget *popover = 0;


static void
PopoverClosed_cb(GtkPopover *p, gpointer user_data) {
    DASSERT(p);

    if(popover) {
        DASSERT(p == GTK_POPOVER(popover));
        gtk_widget_destroy(popover);
        popover = 0;
    }
}


static void MakeAliasFromEntry_cb(GtkEntry *e, void *userData) {

    DASSERT(port);
    DASSERT(!port->graphPortAlias);
    DASSERT(popover);
    DASSERT(port->terminal);
    DASSERT(port->terminal == terminal);
    DASSERT(terminal->drawingArea);

    // name string points to internally allocated storage in the widget
    // and must not be freed, modified or stored.
    const char *name = gtk_entry_get_text(e);

    if(!name || !name[0])
        // Don't try to make a blank name port alias.
        return;

    DSPEW("Got graph port alias entry: \"%s\"", name);

    MakeGraphPortAlias(port, name);

    gtk_widget_destroy(popover);
    popover = 0;

    terminal->needSurfaceRedraw = true;
    gtk_widget_queue_draw(terminal->drawingArea);
}


static inline void MakeGraphAliasWithPopover(const char *name) {

    DASSERT(port);
    DASSERT(!port->graphPortAlias);
    DASSERT(port->terminal);
    DASSERT(port->terminal->drawingArea);
    DASSERT(!popover);

    popover = gtk_popover_new(port->terminal->drawingArea);
    ASSERT(popover);
    GtkWidget *entry = gtk_entry_new();
    if(name) {
        DASSERT(name[0]);
        gtk_entry_set_text(GTK_ENTRY(entry), name);
    }

    gtk_container_add(GTK_CONTAINER(popover), entry);
    g_signal_connect(entry, "activate",
            G_CALLBACK(MakeAliasFromEntry_cb), 0);
    g_signal_connect(popover, "closed",
            G_CALLBACK(PopoverClosed_cb), 0);
    gtk_widget_show(entry);
    gtk_popover_popup(GTK_POPOVER(popover));

    GdkRectangle rec = { 0 };

    int xi = PortIndexToPosition(port->terminal, port->num) + 0.5;
    switch(port->terminal->pos) {

        case Top:
        case Bottom:
            rec.x = xi;
            break;
        case Left:
            rec.y = xi;
            break;
        case Right:
            rec.y = xi;
            rec.x = CON_THICKNESS;
            break;
        case NumPos:
            ASSERT(0);
    }

    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rec);
}


static void MakeAlias_cb(GtkWidget *w, gpointer data) {

    DASSERT(port);
    DASSERT(port->qsPort);
    DASSERT(!port->graphPortAlias);
    DASSERT(port->terminal);
    DASSERT(port->terminal->block);
    DASSERT(port->terminal->block->layout);
    DASSERT(port->terminal->block->layout->qsGraph);
    DASSERT(port->qsPort);

    MakeGraphAliasWithPopover(0);
}


static void RenameAlias_cb(GtkWidget *w, gpointer data) {

    DASSERT(port);
    DASSERT(port->graphPortAlias);
    DASSERT(port->terminal);
    DASSERT(port->terminal->block);
    DASSERT(port->terminal->block->layout);
    DASSERT(port->terminal->block->layout->qsGraph);
    DASSERT(port->qsPort);

    // Now it like the MakeAlias_cb() except that we still need to contend
    // with this string memory of port->graphPortAlias.
    //
    // Save the old graphPortAlias name:
    char *name = strdup(port->graphPortAlias);
    ASSERT(name, "strdup() failed");

    RemoveGraphPortAlias(port);
    DASSERT(!port->graphPortAlias);

    MakeGraphAliasWithPopover(name);

    // We assume that gtk_entry_set_text() copies the string
    // to its' own memory; and we must free name.
#ifdef DEBUG
    memset(name, 0, strlen(name));
#endif
    free(name);
}



static void RemoveAlias_cb(GtkWidget *w, gpointer data) {

    DASSERT(port);
    DASSERT(port->graphPortAlias);
    DASSERT(port->terminal);
    DASSERT(port->terminal->drawingArea);

    RemoveGraphPortAlias(port);
    DASSERT(!port->graphPortAlias);

    terminal->needSurfaceRedraw = true;
    gtk_widget_queue_draw(terminal->drawingArea);
}


static void SetValue_cb(GtkWidget *w, gpointer data) {

    DASSERT(port);
    struct Terminal *t = port->terminal;
    DASSERT(t);
    DASSERT(t->type == Set);
    DASSERT(t->drawingArea);
    DASSERT(port->qsPort);

    // Let's try it with a entry in a popover.
    if(!setValuePopover) {
        setValuePopover = gtk_popover_new(
                port->terminal->drawingArea);
        ASSERT(setValuePopover);
        gtk_widget_set_name(setValuePopover, "port_info");

        DASSERT(setValueEntry == 0);
        DASSERT(setValueLabel == 0);

        GtkWidget *box = gtk_box_new(
                GTK_ORIENTATION_HORIZONTAL, 10/*spacing*/);
        gtk_container_add(GTK_CONTAINER(setValuePopover), box);

        setValueLabel = gtk_label_new("");
        gtk_container_add(GTK_CONTAINER(box), setValueLabel);
        gtk_widget_show(setValueLabel); 

        setValueEntry = gtk_entry_new();
        gtk_container_add(GTK_CONTAINER(box), setValueEntry);
        gtk_widget_show(setValueEntry);
        g_signal_connect(setValueEntry, "activate",
                G_CALLBACK(SetValueEntry_cb), 0);

        gtk_widget_show(box);
    } else {
//DSPEW();
        gtk_popover_set_relative_to(GTK_POPOVER(setValuePopover),
                port->terminal->drawingArea);
//DSPEW();
    }

    const size_t Len = 80;
    char label[Len];
    GetParameterPopoverText(port, label, Len);
    gtk_label_set_text(GTK_LABEL(setValueLabel), label);

    char *entryText = GetParameterValueString((void *)
            port->qsPort);

    gtk_entry_set_text(GTK_ENTRY(setValueEntry), entryText);

#ifdef DEBUG
    memset(entryText, 0, strlen(entryText));
#endif
    free(entryText);

    GdkRectangle rec = { 0 };

    int d = PortIndexToPosition(t, port->num);

    switch(t->pos) {
        case Left:
            rec.y = d;
            break;
        case Right:
            rec.x = CON_THICKNESS;
            rec.y = d;
            break;
        case Top:
            rec.x = d;
            break;
        case Bottom:
            rec.x = d;
            break;
        default:
            ASSERT(0);
            break;
    }

    // This seems to position the popup in an okay way:
    gtk_popover_set_pointing_to(GTK_POPOVER(setValuePopover), &rec);
    gtk_popover_popup(GTK_POPOVER(setValuePopover));

    DSPEW();
}


static void RemoveConnections_cb(GtkWidget *w, gpointer data) {

    DASSERT(port);
    DASSERT(port->terminal);
    DASSERT(port->terminal->drawingArea);
    DASSERT(port->terminal == terminal);

    Disconnect(port);

    DASSERT(port->cons.numConnections == 0);
    DASSERT(port->cons.connections == 0);


    // We can't draw the graph surface until the draw event happens so we
    // use this flag that is seen in the GTK draw callback.
    terminal->block->layout->surfaceNeedRedraw = true;

    terminal->needSurfaceRedraw = true;
    gtk_widget_queue_draw(terminal->drawingArea);
}


static void MoveTerminal_cb(GtkWidget *w, enum Pos pos) {

    DASSERT(terminal);
    DASSERT(terminal->block->layout->layout);
    DASSERT(terminal->drawingArea);


    MoveTerminal(terminal, pos);

    // We can't draw the graph surface until the draw event happens so we
    // use this flag that is seen in the GTK draw callback.
    terminal->block->layout->surfaceNeedRedraw = true;

    terminal->needSurfaceRedraw = true;
    gtk_widget_queue_draw(terminal->drawingArea);
}


static inline
GtkWidget *MakeMenuItem(const char *text,
        void *callback, gpointer userData) {

    DASSERT(menu);

    GtkWidget *mi = gtk_menu_item_new_with_label(text);
    gtk_container_add(GTK_CONTAINER(menu), mi);
    g_signal_connect(mi, "activate", G_CALLBACK(callback), userData);
    gtk_widget_show_all(mi);
    return mi;
}

static inline
void AddSeperator(void) {

    DASSERT(menu);

    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_container_add(GTK_CONTAINER(menu), sep);
    gtk_widget_show(sep);
}



static inline
void CreateTerminalPopupMenu(void) {

    DASSERT(!menu);

    menu = gtk_menu_new();
    ASSERT(menu);

    removeConnections = MakeMenuItem("Remove connection(s)",
            RemoveConnections_cb, 0);
    setValue = MakeMenuItem("Set Value ...", SetValue_cb, 0);
    makeAlias = MakeMenuItem("Make Port Alias ...", MakeAlias_cb, 0);
    renameAlias = MakeMenuItem("Rename Port Alias ...", RenameAlias_cb, 0);
    removeAlias = MakeMenuItem("Remove Port Alias", RemoveAlias_cb, 0);
    AddSeperator();
    left = MakeMenuItem("Switch with LEFT side of block",
            MoveTerminal_cb, (void *) ((intptr_t) Left));
    right = MakeMenuItem("Switch with RIGHT side of block",
            MoveTerminal_cb, (void *) ((intptr_t) Right));
    top = MakeMenuItem("Switch with TOP side of block",
            MoveTerminal_cb, (void *) ((intptr_t) Top));
    bottom = MakeMenuItem("Switch with BOTTOM side of block",
            MoveTerminal_cb, (void *) ((intptr_t) Bottom));

    // TODO: Is this needed?
    g_object_ref(menu);
}


void ShowTerminalPopupMenu(struct Terminal *t, struct Port *p) {

    DASSERT(t);
    DASSERT(t->drawingArea);
    DASSERT(t);

    if(!menu)
        CreateTerminalPopupMenu();

    // There is a case with port not set:  that is the clicking of the
    // mouse pointer on: a terminal with no ports, a point on a terminal
    // between ports, or the edge of the terminal where there are no
    // ports.
    port = p;
    terminal = t;

    if(port && port->cons.numConnections) {
        DASSERT(port->cons.connections);
        gtk_widget_set_sensitive(removeConnections,
                TRUE);
    } else {
        DASSERT(!port || (!port->cons.numConnections &&
                    !port->cons.connections));
        gtk_widget_set_sensitive(removeConnections,
                FALSE);
    }

    gtk_widget_set_sensitive(setValue,
            port && (t->type == Set) &&
            !qsParameter_isGetterGroup((void *) port->qsPort)/*bool*/);

    gtk_widget_set_sensitive(makeAlias,
            (!port || port->graphPortAlias ||
                // Is a stream input port that is connected
                (port->terminal->type == In && port->cons.numConnections)
             )?FALSE:TRUE);

    gtk_widget_set_sensitive(renameAlias,
            (port && port->graphPortAlias)?TRUE:FALSE);

    gtk_widget_set_sensitive(removeAlias,
            (port && port->graphPortAlias)?TRUE:FALSE);

    gtk_widget_set_sensitive(left, TRUE);
    gtk_widget_set_sensitive(right, TRUE);
    gtk_widget_set_sensitive(top, TRUE);
    gtk_widget_set_sensitive(bottom, TRUE);

    // Disable the position the terminal is at now.
    switch(t->pos) {
        case Left:
            gtk_widget_set_sensitive(left, FALSE);
            break;
        case Right:
            gtk_widget_set_sensitive(right, FALSE);
            break;
        case Top:
            gtk_widget_set_sensitive(top, FALSE);
            break;
        case Bottom:
            gtk_widget_set_sensitive(bottom, FALSE);
            break;
        default:
            ASSERT(0);
    }

    gtk_menu_popup_at_pointer(GTK_MENU(menu), 0);
}


void CleanupTerminalPopupMenu(void) {

    if(!menu) return;

    g_object_unref(menu);

    // I do not think this is needed:
    //gtk_widget_destroy(menu);

    menu = 0;
}
