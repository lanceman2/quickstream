//
// This is the source C++ file the is compiled to create a DSO (dynamic
// shared object) library that effectively wraps the Qt family of
// libraries so that we may easily create and link with a process that
// knows nothing about the Qt API.  The running program that loads this
// DSO is not required to be linked with the Qt family of libraries before
// loading this DSO.  Fun with DSOs.
//
/* Compile with (in a bash shell):

HEADERS="$(qtpaths6 --query QT_INSTALL_HEADERS)"
LIBS="$(qtpaths6 --query QT_INSTALL_LIBS)"

g++ -shared -fPIC -g -Wall -Werror qt6_fileLeak_DSO.cpp -o qt6_fileLeak_DSO.so\
 -I${HEADERS}\
 -L${LIBS}\
 -lQt6Widgets -lQt6Core -Wl,-rpath=${LIBS}

# Check it linked well with the Qt versions you expect by running:
ldd qt6_fileLeak_DSO.so

*/

#include <QtWidgets/QtWidgets>

#include "qt6_fileLeak_test.h"

// We need "RunApp" to have a symbol that we can find, among other things
// that extern "C" { provides.
extern "C" {

    void RunApp(int argc, char* argv[]) {

        // TODO: Add a loop around this if you like.  I have done it and
        // see nothing new.

        RunLsOnProcFd("Just before creating QApplication");

#if 1   // Setting #if 1 to #if 0; we notice that
        // #include <QtWidgets/QtWidgets> adds compiled code even when we
        // do not use Qt code in this file (directly).  Another bug?  I'm
        // not even going to try to push that one.  That would just be
        // more pain on me, trying to tell people how to not make resource
        // wasting code.  I must note that, C++ OOP coders tend to have
        // lots of redundant source file includes, so that could be a
        // thing.
        //
        // Compilers and linkers are getting so nice these days that they
        // do not make compiled binaries link with unused code if they do
        // not need to (or so I have observed).  It's like a leaky include
        // header file.

        {
            QApplication qa(argc, argv);
            QWidget window;
            window.resize(620, 740);
            window.show();
            window.setWindowTitle(
                QApplication::translate("toplevel", "Top-level widget"));

            RunLsOnProcFd("Just after creating QApplication");

            printf("\n\nPLEASE CLICK the X (quit button) in the window that pops up\n\n");
        
            qa.exec();
        }

        RunLsOnProcFd("After destroying QApplication");
#endif
    }
}


