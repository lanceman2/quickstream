#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <QtWidgets/QtWidgets>

#include "../lib/debug.h"

#include "qt6_DSO_test.h"


int main(int argc, char* argv[]) {

    const char *DSOPath = "./qt6_DSO_test.so";

    Wait("About to dlopen Qt6 DSO wrapper");

    void *dlh = dlopen(DSOPath, RTLD_LAZY);
    ASSERT(dlh, "dlopen(\"%s\", 0) failed", DSOPath);

    void (*RunApp)(int argc, char* argv[]) =
        (void (*)(int argc, char* argv[])) dlsym(dlh, "RunApp");

    // See if QApplication is robust under reuse.
    RunApp(argc, argv);


    Wait("About to dclose() Qt6 DSO wrapper");

    ASSERT(0 == dlclose(dlh));
    ERROR("dlclose() was called");

    Wait("About to dlopen Qt6 DSO wrapper");


    dlh = dlopen(DSOPath, RTLD_LAZY);
    ASSERT(dlh, "dlopen(\"%s\", 0) failed", DSOPath);

    RunApp(argc, argv);
    // If we did not crash before here than yes we can make create and
    // destroy a QApplication many times in one process.
    //
    // Who knows, QApplication may not even leak memory.

    Wait("About to dclose() Qt6 DSO wrapper");

    ASSERT(0 == dlclose(dlh));
    ERROR("dlclose() was called");

    Wait("About to exit main()");

    return 0;
}
