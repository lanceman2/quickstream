//
// The easiest solution would be that the CSS that makes themes included
// setting the cursor in it; but GTK3 does not support that.  The CSS
// widget theme stuff that GTK3 does support works pretty well; like for
// example controlling the background-color and border of GTK widgets; and
// so we can change that on the fly.
//
// Support for setting the cursor using CSS on web pages just fucking
// works.
//
// The solution may be to see what a "good" working program like firefox
// is doing to control the cursor.  That's a big can of worms.
//
// from looking at /proc/$PID/maps I see a mapping with libgtk-3.so.0.2404.29
// so maybe firefox uses GTK3 to make it's widgets on GNU/Linux.
//

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "../lib/Dictionary.h"

#include "quickstreamGUI.h"


static
struct QsDictionary *dict = 0;


static void CleanupCursor(GdkCursor *cursor) {

    // TODO: WTF  why 2?
    g_object_unref(cursor);
    g_object_unref(cursor);
}


void CleanUpCursors(void) {

    if(!dict) return;
    qsDictionaryDestroy(dict);
    dict = 0;
}


// file:///usr/share/gtk-doc/html/gdk3/gdk3-Cursors.html#gdk-cursor-new-from-name
// 
// cursors we like are: "default", "pointer", "grabbing", "grab"
//
void SetWidgetCursor(GtkWidget *w, const char *name) {

    // First see if we have this cursor already in our dictionary.
    if(!dict)
        dict = qsDictionaryCreate();

    GdkCursor *cursor = qsDictionaryFind(dict, name);

    if(!cursor) {
        cursor = gdk_cursor_new_from_name(
            gdk_display_get_default(),
            name);
        DASSERT(cursor);
        struct QsDictionary *idict;
        ASSERT(0 == qsDictionaryInsert(dict, name, cursor, &idict));
        qsDictionarySetFreeValueOnDestroy(idict,
                (void (*)(void *)) CleanupCursor);
    }

    GdkWindow *win = gtk_widget_get_window(w);
    DASSERT(win);
    gdk_window_set_cursor(win, cursor);
}
