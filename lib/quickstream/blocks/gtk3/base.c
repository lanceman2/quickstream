// This block can be optionally used with other GTK3 widgety blocks from
// this directory to set GTK3 main window attributes.  Without this
// block the user will be default "attributes".

// This file is NOT linked with the GTK3 libraries.

#define _GNU_SOURCE
#include <link.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"
#include "../../../mprintf.h"

#include "all_common.h"
#include "block_common.h"



static struct QsParameter *getter;


static
char *Title_config(int argc, const char * const *argv,
            void *userData) {

    if(argc < 2)
        return 0;

    SetWindowTitle(argv[1]);

    return 0;
}

// Feedback from GtkWidget.
//
// The widget button was clicked.
static void
Destroy(const bool *value) {

    // Tell the graph the window has been destroyed.
    //
    // In our use case, it is possible that the window can be recreated.
    //
    // *value == true means it was destroyed.
    qsGetterPush(getter, value);

    // The X close window button was clicked or other like event.
    //
    // The top level GTK window is now just hidden.  Calling
    // gtk_widget_destory() on the top level main window is a problem
    // as it could cause the process to crash, because GTK3 sucks.
    //
    DSPEW("A window was closed");
}


int declare(void) {

    struct Window *win = CreateWidget(0);

    if(!win) {
        ERROR("You can only load one gtk3/base per graph");
        // The we do not have the win->mutex lock now, given
        // that  CreateWidget(0) failed.
        return -1; // -1 => fail and don't call undeclare()
    }

    qsAddConfig((char *(*)(int argc, const char * const *argv,
            void *userData)) Title_config, "title",
            "set the window title"/*desc*/,
            "title \"My TITLE\"",
            "title Quickstream");

    bool val = false;
    getter = qsCreateGetter("destroy", sizeof(val),
            QsValueType_bool, &val);

    // So the GTK3 window of widgets can tell this block that the window
    // is destroyed.  The Destroy callback is called by 
    win->destroyWindow = qsAddInterBlockJob(
            (void (*)(void *)) Destroy,
            sizeof(val), 7/*queueMax*/);


    // base.so needs to unlock after setting win->destroyWindow above.
    CHECK(pthread_mutex_unlock(win->mutex));

    DSPEW();

    return 0; // success
}


int undeclare(void *userData) {

    // There is no widget, but there is a reference count for this block
    // in the Window.
    DestroyWidget(0);

    DSPEW();

    return 0;
}
