#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdatomic.h>
#include <gtk/gtk.h>

#include "../include/quickstream.h"

#include "../lib/debug.h"
#include "../lib/qsGtk_init.h"

#include "quickstreamGUI.h"



// Clean up some stuff in this program and maybe do somethings before
// exiting.
static inline void FinishUp(void) {

    DSPEW();
}


static void
Catcher(int sig) {

    ASSERT(0, "Caught signal %d\n", sig);
}



int main(int argc, char *argv[]) {

    ASSERT(signal(SIGSEGV, Catcher) != SIG_ERR);
    ASSERT(signal(SIGABRT, Catcher) != SIG_ERR);

    // TODO: Add --help support.

    // qsGtk_init() is a gtk_init() wrapper that helps us use the pthreads
    // API and not the glib gthread stuff.
    //
    qsGtk_init(&argc, &argv);

    AddCSS();

    // Make one top level window to start with.
    //
    // We can make more as it runs.
    if(argc < 2 || !argv[1][0])
        CreateWindow(0);
    else
        CreateWindow(argv[1]);

    gtk_main();

    // TODO: Should this be called before the above loop?
    FinishUp();

    // qsGtk_cleanup() is part of the qsGtk_init() hack.
    qsGtk_cleanup();

    // At this point there will be some system resources not cleaned up by
    // GTK3.  GTK3 is not robust that way.  It's that way by design.
    // There is no corresponding destructor for gtk_init() and/or a
    // library destructor for the GTK3 libraries (crap-ton of
    // libraries).

    // The libquickstream.so resources are all cleaned up in it's library
    // destructor.  We checked it with Valgrind.

    return 0;
}
