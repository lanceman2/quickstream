#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../lib/debug.h"


// this does not return.
void usage(int fd) {

    // This usage() is a little odd.  It launches another program to
    // display the program Usage.  Why?  We put the --help, man pages,
    // together in one program that is also used to generate some of the
    // code that are this program.  This keeps the documentation of the
    // program and it's options consistent.  Really most of the code of
    // this program is in the library libquickstream.so.  The program
    // quickstream should be just a command-line wrapper of
    // libquickstream.so, if that is not the case than we are doing
    // something wrong.

    const char *run = "lib/quickstream/misc/quickstreamHelp";
    ssize_t postLen = strlen(run);
    ssize_t bufLen = 128;
    char *buf = (char *) malloc(bufLen);
    ASSERT(buf, "malloc(%zu) failed", bufLen);
    ssize_t rl = readlink("/proc/self/exe", buf, bufLen);
    ASSERT(rl > 0, "readlink(\"/proc/self/exe\",,) failed");
    while(rl + postLen >= bufLen)
    {
        DASSERT(bufLen < 1024*1024); // it should not get this large.
        buf = (char *) realloc(buf, bufLen += 128);
        ASSERT(buf, "realloc() failed");
        rl = readlink("/proc/self/exe", buf, bufLen);
        ASSERT(rl > 0, "readlink(\"/proc/self/exe\",,) failed");
    }

    buf[rl] = '\0';

    // Now we have something like:
    // buf = "/usr/local/foo/bin/quickstream"

    // remove the "quickstream" part
    ssize_t l = rl - 1;
    // move to a '/'
    while(l && buf[l] != '/') --l;
    if(l) --l;
    // move to another '/'
    while(l && buf[l] != '/') --l;
    if(l == 0) {
        printf("quickstreamHelp was not found\n");
        exit(1);
    }

    // now buf[l] == '/'
    // add the "lib/quickstream/misc/quickstreamHelp"
    strcpy(&buf[l+1], run);

    //fprintf(stderr, "running: %s\n", buf);

    if(fd != STDOUT_FILENO)
        // Make the quickstreamHelp write to stderr.
        dup2(fd, STDOUT_FILENO);

    pid_t pid = fork();

    if(pid == -1) {
        fprintf(stderr, "fork() failed\n");
        exit(1);
    }

    if(pid == 0) {
        // child
        execl(buf, buf, "-h", NULL);
        fprintf(stderr, "execl(\"%s\",,) failed\n", buf);
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
