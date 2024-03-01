// This test program shows the Qt is more robust than GTK.  The main loop
// thingy of Qt has a destructor and GTK does not.  GTK can have only one
// main loop object in a process; they never bothered to make a gtk_init()
// a corresponding destructor.
//
// I wonder how thread-safe QApplication is: Can I make many threads,
// each with a different QApplication in it?

// From
// https://doc.qt.io/qt-6/qtwidgets-tutorials-widgets-toplevel-example.html
// and some changes.

// We are not using gmake, Cmake, or whatever other build tools that Qt6
// uses; so we do not have a standard header include like in the Qt
// examples:
#include <QtWidgets/QtWidgets>

#include "../lib/debug.h"


static
int RunWindow(QApplication *qa) {

    QWidget window;
    window.resize(620, 740);
    window.show();
    window.setWindowTitle(
        QApplication::translate("toplevel", "Top-level widget"));
    return qa->exec();
}

static
void RunApp(int argc, char* argv[]) {

    // We tried making 2 QApplications and it just hangs the second
    // QApplication constructor.  We'll need to make a wrapper of
    // QApplication, so that we can count them, and pretend to make many
    // of them.  Far better than the bull shit we had to do to use GTK3 as
    // a module.
    //QApplication qa2(argc, argv);
    QApplication qa(argc, argv);

    ERROR();
    ERROR("exec() returned %d", RunWindow(&qa));
    ERROR("exec() returned %d", RunWindow(&qa));
}


int main(int argc, char* argv[]) {

    // See if QApplication is robust under reuse.
    RunApp(argc, argv);

    RunApp(argc, argv);
    // If we did not crash before here than yes we can make create and
    // destroy a QApplication many times in one process.
    //
    // Who knows, QApplication may not even leak memory.
    return 0;
}
