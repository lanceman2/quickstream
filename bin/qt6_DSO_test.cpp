#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>

#include <QtWidgets/QtWidgets>

#include "../lib/debug.h"

#include "qt6_DSO_test.h"


static
int RunWindow(QApplication *qa) {

    QWidget window;
    window.resize(620, 740);
    window.show();
    window.setWindowTitle(
        QApplication::translate("toplevel", "Top-level widget"));

    DSPEW();

    Wait("About to call qa->exec()");

    return qa->exec();
}


void RunApp(int argc, char* argv[]) {

    // We tried making 2 QApplications and it just hangs the second
    // QApplication constructor.  We'll need to make a wrapper of
    // QApplication, so that we can count them, and pretend to make
    // many of them.  Far better than the bull shit we had to do to
    // use GTK3 as a module.
    //QApplication qa2(argc, argv);
    QApplication qa(argc, argv);

    ERROR();
    ERROR("exec() returned %d", RunWindow(&qa));
    ERROR("exec() returned %d", RunWindow(&qa));
}

