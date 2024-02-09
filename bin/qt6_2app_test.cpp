// This test program shows the Qt is more robust than GTK.  The main loop
// thingy of Qt has a destructor and GTK does not.  GTK can have only one
// main loop object in a process; they never bothered to make a gtk_init()
// a corresponding destructor.
//
// I wonder how thread-safe QApplication is: Can I make many threads,
// each with a different QApplication in it?

#include <QtWidgets/QtWidgets>

#include "../lib/debug.h"

// From
// https://doc.qt.io/qt-6/qtwidgets-tutorials-widgets-toplevel-example.html
// and some changes.

int RunWindow(QApplication *qa) {

    QWidget window;
    window.resize(320, 240);
    window.show();
    window.setWindowTitle(
        QApplication::translate("toplevel", "Top-level widget"));
    return qa->exec();
}

void RunApp(int argc, char* argv[]) {

    QApplication qa(argc, argv);

    // See if QApplication is robust under reuse.
    ERROR("exec() returned %d", RunWindow(&qa));
    ERROR("exec() returned %d", RunWindow(&qa));
}


int main(int argc, char* argv[]) {

    RunApp(argc, argv);
    RunApp(argc, argv);

    return 0;
}
