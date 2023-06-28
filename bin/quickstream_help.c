#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/wait.h>

#include "../lib/debug.h"

#include "./quickstream.h"

#include "../include/quickstream.h"


// this does not return in the current process that calls this.
void help(int fd, const char *opt) {

    // This usage, --help thing, is a little odd.  It launches another
    // program to display the program help and usage.  Why?  We put the
    // --help, man pages, together in one program that is also used to
    // generate some of the code that are in this program.  This keeps the
    // documentation of the program and it's options consistent.  Really
    // most of the code of this program is in the library
    // libquickstream.so.  The program quickstream should be just a
    // command-line wrapper of libquickstream.so, if that is not the case
    // than we are doing something wrong.

    const char *run = "/quickstream/misc/quickstreamHelp";

    size_t len = strlen(qsLibDir) + strlen(run) + 1;
    char *path = calloc(1, len);
    ASSERT(path, "calloc(1,%zu) failed", len);
    snprintf(path, len, "%s%s", qsLibDir, run);

    if(access(path, R_OK | W_OK) != 0) {
        // TODO: We considered keeping on running after this failure, but
        // this is simpler and, we think, expected behavior.
        fprintf(stderr, "program quickstreamHelp was not found in \"%s\"\n",
                path);
        // If ever we could check for memory leaks, we cleanup.
        free(path);
        // FAIL
        exit(1);
    }

    if(fd != STDOUT_FILENO)
        // Make the quickstreamHelp write to stderr.
        dup2(fd, STDOUT_FILENO);

    pid_t pid = fork();

    if(pid == -1) {
        fprintf(stderr, "fork() failed\n");
        exit(1);
    }

    if(pid == 0) {
        // I am the child
        //
        // Valgrind says this (buf) leaks memory; but clearly it can't be
        // helped and does not matter; the OS cleans up for us.
        //
        // opt = "-H" for --help  and "-u" for --usage
        //
        execl(path, path, opt, NULL);
        fprintf(stderr, "execl(\"%s\",,) failed\n", path);
        exit(1); // non-zero error code, fail.
    }

    // I am the parent
    int statval = 1;
    wait(&statval);
    if(WIFEXITED(statval))
        exit(WEXITSTATUS(statval));
    fprintf(stderr, "Child did not terminate with exit\n");
    exit(1);
}
