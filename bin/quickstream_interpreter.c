// This program, quickstream_interpreter, is to get around the limitations
// of GNU/Linux Shebang in some use cases; it's like the program
// "quickstream" with this one C file changed.
//
// You see we need to provide a way for quickstream users to run a
// quickstream graph via the common Shebang at the top of a file like:
//
//    #!/usr/bin/env quickstream_interpreter
//
// because something like:
//
//    #!/usr/bin/env quickstream --interpreter
//
// will not work because the 2 arguments "quickstream --interpreter"
// end up being seen as 1 argument.  The "quickstream --interpreter"
// is not the program we wish to run, "quickstream" is.
// Running: "quickstream_interpreter" is equivalent to running
// "quickstream --interpreter", that is "quickstream" with the
// "--interpreter" command-line option.
//
//
//   See: https://en.wikipedia.org/wiki/Shebang_(Unix)
//
//
// It also gets around the limitations of the length of a bash command
// line.  The quickstream_interpreter file has no file size limit, where
// as a single bash command line has a length limit where-by limiting the
// number of quickstream commands that can be run by the "quickstream"
// program, without this interpreter mode.
//
//
// TODO: A little more studying of what this does is needed.
//
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/debug.h"

#include "../include/quickstream.h"

#include "./quickstream.h"

// defines qsOptions[] and DEFAULT_SPEW_LEVEL
#include "../lib/quickstream/misc/qsOptions.h"



int exitStatus = 0;

int spewLevel = DEFAULT_SPEW_LEVEL;

bool exitOnError = false;



static void gdb_catcher(int signum) {
    // The action of ASSERT() depends on how this is compiled.
    ASSERT(0, "Caught signal %d\n", signum);
}


int main(int argc, const char * const *argv) {

    qsSetSpewLevel(spewLevel);

    // Hang the program for debugging, if we segfault.
    ASSERT(SIG_ERR != signal(SIGSEGV, gdb_catcher));

    return RunInterpreter(argc-1, argv+1);
}
