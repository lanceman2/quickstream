// TODO: Add a non-blocking read version/option that uses the qs
// epoll_wait(2) thread.

// quickstream (built-in) Block that launches a program at construct() or
// start(), then writes a pipe which may be read by the program.
//
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include "../../debug.h"
#include "../../parseBool.h"
#include "../../../include/quickstream.h"
#include "../../mprintf.h"





#define DEFAULT_INPUTMAX 2048
#define STR(s)   XSTR(s)
#define XSTR(s)  #s



// We allocate a PipeOut for each PipeOut block loaded.
struct PipeOut {

    int fd;          // file descriptor
    char **program;  // program to run
    char *Path;      // used for PATH at program launch
    pid_t childPid;
    int signalNum;   // signal at stop() or destroy() 0 => none
    bool atStart;    // or at construct()
    bool doWait;
};



static inline void CleanUpFd(struct PipeOut *p) {

    DASSERT(p);

    if(p->fd > -1) {
        close(p->fd);
        p->fd = -1;
    }
}


static inline void CleanUpProgram(struct PipeOut *p) {

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


static void inline CleanUpPath(struct PipeOut *p) {

    DASSERT(p);

    if(p->Path) {
#ifdef DEBUB
        memset(p->Path, 0, strlen(p->path));
#endif
        free(p->Path);
        p->Path = 0;
    }
}


static
char *InputMax_config(int argc, const char * const *argv,
        struct PipeOut *p) {

    DASSERT(p);

    size_t inputMax = qsParseSizet(DEFAULT_INPUTMAX);

    if(inputMax < 1)
        inputMax = 1;

    qsSetInputMax(0, inputMax);

    return 0;
}


static
char *SignalNum_config(int argc, const char * const *argv,
        struct PipeOut *p) {

    DASSERT(p);

    p->signalNum = qsParseInt32t(p->signalNum);

    if(p->signalNum < 0)
        p->signalNum = 0;

    return 0;
}


static
char *AtStart_config(int argc, const char * const *argv,
        struct PipeOut *p) {

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
        struct PipeOut *p) {

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


static
char *RelativePath_config(int argc, const char * const *argv,
        struct PipeOut *p) {

    if(argc < 2 || !argv[1][0])
        return QS_CONFIG_FAIL;

    CleanUpPath(p);

    p->Path = strdup(argv[1]);
    ASSERT(p->Path, "strdup() failed");

    return mprintf("RelativePath %s", p->Path);
}


int PipeOut_declare(void) {


    struct PipeOut *p = calloc(1, sizeof(*p));
    ASSERT(p, "calloc(1,%zu) failed", sizeof(*p));

    // Defaults
    p->fd = -1;
    p->signalNum = SIGTERM;
    p->doWait = true;

    qsSetUserData(p);

    // This block is a sink with a single input stream
    qsSetNumInputs(1, 1);

    qsSetInputMax(0/*port*/, DEFAULT_INPUTMAX);


    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            InputMax_config, "InputMax",
            "Bytes written per flow() call.",
            "InputMax BYTES",
            "InputMax " STR(DEFAULT_INPUTMAX));

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

    char *helpText = mprintf(
            "Prepend a directory to the PATH used to launch the program. "
            "If the directory given is not a full file path, prepend the "
            "library run directory of libquickstream.so which is "
            "currently %s.  The current PATH is %s",
            qsLibDir, getenv("PATH"));

    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            RelativePath_config, "RelativePath", helpText,
            "RelativePath PATH",
            "RelativePath ");
#ifdef DEBUG
    memset(helpText, 0, strlen(helpText));
#endif
    free(helpText);


    return 0; // success
}


static void SignalCatcher(int sig) {

    INFO("----------------caught signal %d", sig);
}


static inline int
Start(struct PipeOut *p) {

    DASSERT(p);
    ASSERT(p->program, "Program to run was not set");
    DASSERT(p->childPid == 0);
    DASSERT(p->fd < 0);

    DSPEW("Starting program: %s", p->program[0]);

    int fd[2];
    ASSERT(pipe(fd) == 0);

    {
        struct sigaction act = {
            .sa_handler = SignalCatcher,
            .sa_flags = SA_NODEFER
        };
        //
        // I think we have to either catch these or block them.  If we
        // don't the program will just be terminated do to the signals
        // default action.  Now if the program receiving the piped data do
        // not generate these signals, then ya...
        //
        // I'll try just catching them, and doing nothing.  We handle the
        // write() error broken pipe and we get the return status of the
        // child.
        CHECK(sigaction(SIGCHLD, &act, 0));
        CHECK(sigaction(SIGPIPE, &act, 0));
    }

    p->childPid = fork();
    ASSERT(p->childPid >= 0, "fork() failed");
    if(p->childPid) {
        // I'm the parent.
        p->fd = fd[1];
        close(fd[0]);
        DSPEW("Parent process starting child to run "
                "\"%s\" complete", p->program[0]);
        return 0;
    }

    // else I'm the child:
    close(fd[1]);
    DASSERT(STDIN_FILENO == 0);
    ASSERT(dup2(fd[0], STDIN_FILENO) != -1);

    if(p->Path) {

        char *path;

        // TODO: Linux specific code here:
        char *currentPath = getenv("PATH");
        if(p->Path[0] != '/') {
            if(currentPath)
                path = mprintf("%s/%s:%s", qsLibDir,
                        p->Path, currentPath);
            else
                // WTF no current PATH?  Oh well.
                path = mprintf("%s/%s", qsLibDir, p->Path);
        } else {
            if(currentPath)
                path = mprintf("%s:%s", currentPath, p->Path);
            else
                // WTF no current PATH?  Oh well.
                path = p->Path;
        }

        ASSERT(0 == setenv("PATH", path, 1/*overwrite*/));
    }

    execvp(p->program[0], p->program);

    ERROR("execlp(\"%s\",,0) failed", p->program[0]);

    return -1;
}


static inline int
Stop(struct PipeOut *p) {

    DASSERT(p);
    DASSERT(p->program);
    DASSERT(p->childPid);
    DASSERT(p->fd >= 0);

    close(p->fd);

    CHECK(sigaction(SIGCHLD, 0, 0));
    CHECK(sigaction(SIGPIPE, 0, 0));

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


int PipeOut_construct(struct PipeOut *p) {

    if(p->program && !p->atStart)
        return Start(p);
 
    return 0;
}


int PipeOut_start(uint32_t numInputs, uint32_t numOutputs,
        struct PipeOut *p) {

    DASSERT(p);

    if(p->program && p->atStart)
        return Start(p);

    return 0;
}


int PipeOut_flow(const void * const in[], const size_t inLens[],
        uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        struct PipeOut *p) {

    DASSERT(p->fd >= 0);

    if(!(*inLens))
        // Got no input.
        return 0;

    ssize_t ret = write(p->fd, *in, *inLens);

    if(ret <= 0) {
        if(ret < 0)
            WARN("write(%d,%p,%zu)=%zd failed", p->fd, *in, *inLens, ret);
        else
            // Returned 0.  What does that mean?
            INFO("write(%d,%p,%zu) returned 0", p->fd, *in, *inLens);
        return 1; // done for this flow cycle.
    }

    qsAdvanceInput(0, ret);

    return 0; // success
}


int PipeOut_stop(uint32_t numInputs, uint32_t numOutputs,
        struct PipeOut *p) {

    DSPEW();

    if(p->program && p->atStart)
        return Stop(p);

    return 0;
}


int PipeOut_destroy(struct PipeOut *p) {

    if(p->program && !p->atStart)
        return Stop(p);

    return 0;
}


int PipeOut_undeclare(struct PipeOut *p) {

    DASSERT(p);
    DSPEW();

    CleanUpProgram(p);
    CleanUpFd(p);
    CleanUpPath(p);

#ifdef DEBUG
    memset(p, 0, sizeof(*p));
#endif
    free(p);

    return 0;
}
