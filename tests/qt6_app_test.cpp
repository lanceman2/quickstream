// Tested on Debian GNU/Linux 12 (bookworm)
// Latest as of around February 2024
//
// Used a local git repo build of Qt6 from
//
//
// You just need this one C++ source file:
// qt6_app_test.cpp and some common Debian packages.
//
// I wanted to keep this test to just one source file, so no
// build/make/cmake/qmake (whatever) files come with this test.
//
//
/* Compile with (in a bash shell):

HEADERS="$(qtpaths6 --query QT_INSTALL_HEADERS)"
LIBS="$(qtpaths6 --query QT_INSTALL_LIBS)"

g++ -g -Wall -Werror qt6_app_test.cpp -o qt6_app_test\
 -I${HEADERS}\
 -L${LIBS}\
 -lQt6Widgets -lQt6Core -Wl,-rpath=${LIBS}

# Check it linked well with (the Qt versions I expect):
ldd qt6_app_test

*/
// I just cut and pasted it, and it worked for me.  I hate it
// when people do not test their scripts.  No guarantee it will
// work for you, but I did cut and past, and run it.
// ldd showed it to be linked to a very new version of Qt6 that
// I built from current source (in my case):
//
// >> git log
//
// commit 18287421fd31a9464c66a00f5f8b404e9af771ef
//  (HEAD -> dev, origin/dev, origin/HEAD)
// Author: Dominik Holland <dominik.holland@qt.io>
// Date:   Thu Feb 22 13:41:50 2024 +0100
//
// New enough I'd guess.
//
//
// One could imagine adding this test to one of the Qt test suites and
// have it fail based on whither the child sees to many files listed in
// /proc/PID/fd/, have the child exit with a failure status and than the
// parent gets that and in turn fails.  The creating of the window could
// be removed or the window could be closed just after it's created.
// There may be a lot of ways to make this non interactive.  For now this
// test is interactive.  I do not understand Qt test suites (or whatever
// they are called) well enough to make this a "Qt test".
//
// If there is interest in this "bug", I have more related bugs that I can
// share.  I don't expect a positive response from this "bug".  That is,
// I don't think that Qt developers will consider this is a bug.  But this
// at least points out that the QApplication (and family of classes) are
// not robust in the UNIX sense.
//
// How I came about this:  See
// https://forum.qt.io/topic/154863/dso-plugin-qt6-widgets/
// DSO Plugin Qt6 Widgets
//


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <QtWidgets/QtWidgets>
// Could be if using Qt/cmake build like stuff then maybe we need:
//#include <QtWidgets>


static
void RunLsOnProcFd(const char *label) {

    pid_t parentPid = getpid();

    pid_t pid = fork();
    if(pid == -1) {
        perror("fork() FAILED\n");
        exit(1);
    }
    if(pid) {
        // I'm the parent and I'll wait for the child
        // to run "ls".
        if(pid != waitpid(pid, 0, 0)) {
            perror("waidpid() FAILED\n");
            exit(1);
        }
        return;
    }

    // I'm the child that will run "ls"

    // PID numbers are only so long.
    //
    char *dir = strdup("/proc/PARENT_PID_AAAAAAAAAAAA/fd/");
    if(!dir) {
        perror("strdup() FAILED\n");
        exit(1);
    }
    size_t len = strlen(dir);
    snprintf(dir, len, "/proc/%d/fd/", parentPid);

    printf("\n----- %s list of open files from %s --------------\n",
            label, dir);

    execlp("ls", "-lt", "--color=auto", "--file-type", "--full-time",
            dir, NULL);
    perror("execlp() FAILED\n");
    exit(1);
}


static
void RunApp(int argc, char* argv[]) {

    //QCoreApplication qa(argc, argv);
    // Leads to spew like:
    // QWidget: Cannot create a QWidget without QApplication
    //
    // So it looks like we need QApplication which inherits
    // QCoreApplication.
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



int main(int argc, char* argv[]) {

    // Note: this test is a lot easier than using valgrind to test of file
    // leaks.  It's a little most more bulletproof than valgrind can be,
    // but requires more test code here in this file.

    // Run "ls" to list all open files for the process now:
    RunLsOnProcFd("Before creating QApplication");

    // Make and destroy a QApplication with window.
    //
    // This will print instructions to the user to stdout telling
    // them to X (close) the window that popped up.
    //
    // I don't know that the window is required, but that seemed
    // like a reasonable minimum thing to do.
    RunApp(argc, argv);

    // Run "ls" to list all open files for the process now.
    //
    // Note, there should only have been 3 files open at this
    // time stdin, stdout, and stderr.  The QApplication object
    // has been destroyed by now.
    RunLsOnProcFd("After destroying QApplication");

    printf("\nThere should have only have been 3 files"
            " open at most: 0, 1, and 2\n"
            "The other files you see are from "
            "QApplication leaking files.\n");

    // If there is a race condition in the QApplication object destructor,
    // one could wait a little and then check the file list again.  I
    // never write code with threads that race, but some people do so I'll
    // test for it (at least a 1 second wait) if you change the #if 0 to
    // #if 1.   Also, how the hell does /proc stay synced, I'm guessing the
    // kernel makes the whole /proc file system seem like it's in sync
    // with any process that accesses it.  Is that true?  I can't imagine
    // that is not so.  Linux is not a piece of shit, it's pretty dam
    // robust.
    //
    // Also note, there could also be a race condition that is overcome in
    // the next QApplication creation.  There are just so many
    // possibilities of leaky code that we can't test them all.
    //
    // I see no change after waiting 1 second.
#if 0
    printf("\nSleeping 1 second\n\n");
    sleep(1);
    RunLsOnProcFd("Way after destroying QApplication");
#endif

    return 0;
}
