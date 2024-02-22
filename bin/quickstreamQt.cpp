// This is like quickstreamGUI but with Qt 6 and not GTK3.

#include <inttypes.h>
//#include <string.h>
//#include <stdlib.h>
//#include <pthread.h>


#include <QtWidgets/QtWidgets>

#include "../lib/qsQtApp.h"
#include "../lib/debug.h"
#include "../include/quickstream.h"

#include "qsQt.h"


int main(int argc, const char * const *argv) {

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
