// This is like quickstreamGUI but with Qt 6 and not GTK3.

#include <inttypes.h>
#include <errno.h>
//#include <string.h>
//#include <stdlib.h>
//#include <pthread.h>


#include <QtWidgets/QtWidgets>

#include "../lib/qsQtApp.h"
#include "../lib/debug.h"
#include "../include/quickstream.h"

#include "qsQt.h"


static void
Catcher(int sig) {

    ASSERT(0, "Caught signal %d\n", sig);
}

int main(int argc, const char * const *argv) {

    errno = 0;
    ASSERT(signal(SIGSEGV, Catcher) != SIG_ERR);
    ASSERT(signal(SIGABRT, Catcher) != SIG_ERR);

    DSPEW();

    // qsQtApp is a wrapper of QApplication.  It is needed so that we can
    // have quickstream blocks that make Qt widgets without having to have
    // Qt libraries linked to ../lib/libquickstream.so.
    qsQtApp_construct();

    // Add block path argument if it's given in the command-line.
    CreateWindow();

    qsQtApp_exec();

    qsQtApp_destroy();
    return 0;
}
