// This is the source code to a built-in block the is compiled into
// lib/libquickstream.so
//
// TODO: Add a non-blocking read version/option that uses the qs
// epoll_wait(2) thread.
//
// quickstream built-in block that launches a program at construct() or
// start(), then reads a pipe which may be feed by the program.

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include "../../../debug.h"
#include "../../../parseBool.h"

#include "../../../../include/quickstream.h"



#define DEFAULT_OUTPUTMAX 2048
#define STR(s)   XSTR(s)
#define XSTR(s)  #s


// We allocate a PipeIn for each PipeIn block loaded.
struct PipeIn {

    int fd;          // file descriptor
    char **program;  // program to run
    pid_t childPid;
    int signalNum;   // signal at stop() or destroy() 0 => none
    bool atStart;    // or at construct()
    bool doWait;
};



static void CleanUpProgram(struct PipeIn *p) {

    DASSERT(p);

    if(p->program) {

        for(char **str = p->program; *str; ++str) {
#ifdef DEBUG
            memset(*str, 0, strlen(*str));
#endif
            free(*str);
#ifdef DEBUG
            *str = 0;
#endif
        }
        free(p->program);
        p->program = 0;
    }
}


static void CleanUpFd(struct PipeIn *p) {

    DASSERT(p);

    if(p->fd > -1) {
        close(p->fd);
        p->fd = -1;
    }
}


static
char *OutputMax_config(int argc, const char * const *argv,
        struct PipeIn *p) {

    DASSERT(p);

    size_t outputMax = qsParseSizet(DEFAULT_OUTPUTMAX);

    if(outputMax < 1)
        outputMax = 1;
 
    qsSetOutputMax(0, outputMax);

    return 0;
}


static
char *SignalNum_config(int argc, const char * const *argv,
        struct PipeIn *p) {

    DASSERT(p);

    p->signalNum = qsParseInt32t(p->signalNum);

    if(p->signalNum < 0)
        p->signalNum = 0;

    return 0;
}


static
char *AtStart_config(int argc, const char * const *argv,
        struct PipeIn *p) {

    DASSERT(p);

    if(argc < 2) {
        p->atStart = true;
        return 0;
    }

    p->atStart = qsParseBool(argv[1]);

    return 0;
}


static
char *Program_config(int argc, const char * const *argv,
        struct PipeIn *p) {

    DASSERT(p);

    if(argc < 2)
        // Kind of pointless of the user to do this?
        return 0;

    if(p->program)
        CleanUpProgram(p);


    p->program = calloc(argc, sizeof(*p->program));
    ASSERT(p->program, "calloc(%d,%zu) failed", argc, sizeof(*p->program));

    for(int i=1; i<argc; ++i) {
        p->program[i-1] = strdup(argv[i]);
        ASSERT(p->program[i-1], "strdup() failed");
    }
    // Terminate the array of strings with a Null terminator.
    p->program[argc-1] = 0;

#if 0
    fprintf(stderr, "program=");
    for(char **s = p->program; *s; ++s)
        fprintf(stderr, " %s", *s);
    fprintf(stderr, "\n\n");
WARN("p=%p p->childPid=%u", p, p->childPid);
#endif

    return 0; // success
}



int PipeIn_declare(void) {

    struct PipeIn *p = calloc(1, sizeof(*p));
    ASSERT(p, "calloc(1,%zu) failed", sizeof(*p));

    // Defaults
    p->fd = -1;
    p->signalNum = SIGTERM;
    p->doWait = true;

    qsSetUserData(p);

    // This block is a source with a single output stream
    qsSetNumOutputs(1, 1);

    qsSetOutputMax(0, DEFAULT_OUTPUTMAX);


    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            OutputMax_config, "OutputMax",
            "Bytes written per flow() call.",
            "OutputMax BYTES",
            "OutputMax " STR(DEFAULT_OUTPUTMAX));


    char defaultSigNum[32];
    snprintf(defaultSigNum, 32, "SignalNum %d", SIGTERM);

    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            SignalNum_config, "SignalNum",
            "Signal the program, 0 is to not signal",
            "SignalNum NUM", defaultSigNum);

    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            AtStart_config, "AtStart",
            "Setup to launch the program at each block start(). "
            "Otherwise the program will be launched at block construct()",
            "AtStart [BOOL]",
            "AtStart False"/*Not Set by default*/);

    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            Program_config, "Program",
            "Set the program to launch.  Program must be set",
            "Program COMMAND [ARG1 ...]",
            "Program cat");


    return 0; // success
}


static inline int
Start(struct PipeIn *p) {

    DASSERT(p);
    ASSERT(p->program, "Program to run was not set");
    DASSERT(p->childPid == 0);
    DASSERT(p->fd < 0);

    int fd[2];
    ASSERT(pipe(fd) == 0);

    p->childPid = fork();
    ASSERT(p->childPid >= 0, "fork() failed");
    if(p->childPid) {
        // I'm the parent.
        p->fd = fd[0];
        close(fd[1]);
        return 0;
    }

    // else I'm the child:
    close(fd[0]);
    DASSERT(STDOUT_FILENO == 1);
    ASSERT(dup2(fd[1], STDOUT_FILENO) != -1);

    execvp(p->program[0], p->program);

    ERROR("execlp(\"%s\",,0) failed", p->program[0]);

    return -1;
}


static inline int
Stop(struct PipeIn *p) {

    DASSERT(p);
    DASSERT(p->program);
    DASSERT(p->childPid);
    DASSERT(p->fd >= 0);

    close(p->fd);

    if(p->signalNum)
        if(kill(p->childPid, p->signalNum))
            WARN("kill(%u,%d) failed", p->childPid, p->signalNum);

    if(p->doWait) {
        int status = 0;
        pid_t pid = waitpid(p->childPid, &status, 0);
        if(pid < 0)
            WARN("waitpid(%u,,0) returned %u", p->childPid, pid);
        else
            DSPEW("waitpid(%u,,0) returned status %d",
                    p->childPid, status);
    }

    // Reset values.
    p->childPid = 0;
    p->fd = -1;

    return 0;
}



int PipeIn_construct(struct PipeIn *p) {

    if(p->program && !p->atStart)
        return Start(p);
 
    return 0;
}


int PipeIn_start(uint32_t numInputs, uint32_t numOutputs,
        struct PipeIn *p) {

    DASSERT(p);

    if(p->program && p->atStart)
        return Start(p);

    return 0;
}


int PipeIn_flow(const void * const in[], const size_t inLens[],
        uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        struct PipeIn *p) {

    DASSERT(p->fd >= 0);

    if(!(*outLens))
        // Got no room to write output to.
        return 0;


    ssize_t ret = read(p->fd, *out, *outLens);

    if(ret <= 0) {
        if(ret < 0)
            WARN("read(%d,%p,%zu)=%zd failed", p->fd, *out, *outLens, ret);
        else
            // Returned 0.  End of file, so I guess.
            INFO("read(%d,%p,%zu) returned 0", p->fd, *out, *outLens);
        return 1; // done for this flow cycle.
    }

    qsAdvanceOutput(0, ret);

    return 0; // success
}


int PipeIn_stop(uint32_t numInputs, uint32_t numOutputs,
        struct PipeIn *p) {

    if(p->program && p->atStart)
        return Stop(p);

    return 0;
}


int PipeIn_destroy(struct PipeIn *p) {

    if(p->program && !p->atStart)
        return Stop(p);

    return 0;
}


int PipeIn_undeclare(struct PipeIn *p) {

    DASSERT(p);
    DSPEW();

    CleanUpProgram(p);
    CleanUpFd(p);

#ifdef DEBUG
    memset(p, 0, sizeof(*p));
#endif
    free(p);

    return 0;
}
