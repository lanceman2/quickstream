// This is compiled into a DSO (dynamic shared object) that is
// loadable with dlopen(3) and is executable.
//
// Executing the binary made from this file executes ../bin/quickstream
// which loads the DSO from this file as a block module.
//
// So using this trick ... I should be able to make DSO (dynamic shared
// object) quickstream super blocks that can run themselves.


// TODO: This is, so very much, not portable code.


//https://stackoverflow.com/questions/1449987/building-a-so-that-is-also-an-executable

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "../lib/debug.h"
#include "../include/quickstream.h" // for QS_DL_LOADER

/*
   In a bash shell you can run:

   /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 --help
*/

const char service_interp[] __attribute__((section(".interp"))) = QS_DL_LOADER;

// may be needed on i386 computers:
//__attribute__((force_align_arg_pointer))
int main(int argc, char **argv) {

    printf("hello\n");

    ASSERT(setenv("QS_BLOCK_PATH", ".", 1) == 0);

    execl("../bin/quickstream",
            "quickstream", "--exit-on-error",
            "--block", "471_execBlock.so", NULL);
    ERROR("execl() failed");
    return 1;
}


int declare(void) {

    DSPEW();

    return 0;
}
