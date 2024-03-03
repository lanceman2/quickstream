// Ref:  https://bugreports.qt.io/browse/QTBUG-122948
//
// This is just a test.  We are not looking to optimise this code.
//
// This test has 3 source files required: qt6_fileLeak_test.cpp (this
// file), qt6_fileLeak_DSO.cpp, and qt6_fileLeak_test.h
//

/* Compile with (in a bash shell):

g++ -g -Wall -Werror qt6_fileLeak_test.cpp -o qt6_fileLeak_test

# Check it linked well without Qt6 libraries:
ldd qt6_fileLeak_test

# I get the following output:
#
#      linux-vdso.so.1 (0x00007ffd7d7dc000)
#      libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f498e49d000)
#      /lib64/ld-linux-x86-64.so.2 (0x00007f498e6a5000)
#


*/

// TODO: Add counting the number of files leaked in a given run.
//
// I've counted 12 every time I've run the loop.



#include <dlfcn.h>

#include "qt6_fileLeak_test.h"


static inline void Run(int argc, char* argv[]) {

    // Run "ls" to list all open files for the process now:
    RunLsOnProcFd("Before loading QApplication DSO");

    const char *dsoFilename = "./qt6_fileLeak_DSO.so";

    dlerror();
    void *hdl = dlopen(dsoFilename, RTLD_NOW);
    if(!hdl) FAIL("dlopen(\"%s\",)-- %s --", dsoFilename, dlerror());

    RunLsOnProcFd("After loading QApplication DSO and before creating QApplication");

    void (*RunApp)(int argc, char* argv[]) =
        (void (*)(int argc, char* argv[])) dlsym(hdl, "RunApp");

    if(!RunApp) FAIL("dlsym(,RunApp)");

    RunApp(argc, argv);

    if(dlclose(hdl) != 0) FAIL("dlclose(%p)", hdl);

    RunLsOnProcFd("After unloading QApplication DSO");

    printf(
        "NOTE: there appears to be 12 files left open that were created\n"
        "      by creating a QApplication object (you need to count yourself).\n\n");
}


int main(int argc, char* argv[]) {

    const int NUM_LOOPS = 2;

    for(int i=0; i < NUM_LOOPS;) {
        Run(argc, argv);
        if(++i < NUM_LOOPS) {
            printf("<enter> to run again\n");
            getchar();
        }
    }

    return 0;
}
