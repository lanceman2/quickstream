#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "../lib/debug.h"
#include "../include/quickstream/app.h"

#include "getOpt.h"

// From quickstream_usage.c
extern int usage(int fd);


static void gdb_catcher(int signum) {

    // The action of ASSERT() depends on how this is compiled.
    ASSERT(0, "Caught signal %d\n", signum);
}


int main(int argc, const char * const *argv) {

    // Hang the program for debugging, if we segfault.
    signal(SIGSEGV, gdb_catcher);

    DSPEW("QUICKSTREAM_VERSION=\"%s\"", QUICKSTREAM_VERSION);

    struct QsApp *app = qsAppCreate();


    qsAppDestroy(app);

    usage(STDERR_FILENO);

    return 0;
}
