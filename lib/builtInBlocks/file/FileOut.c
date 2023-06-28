// TODO: Add a non-blocking read version/option that uses the qs
// epoll_wait(2) thread.

// quickstream (built-in) Block that is a sink that writes to a file using
// system call write(2).  This does not use the standard buffered file
// stream.  The user may set the file descriptor or a file path to
// open(2).

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>

#include "../../debug.h"
#include "../../mprintf.h"
#include "../../../include/quickstream.h"


#define DEFAULT_INPUTMAX 2048
#define STR(s)   XSTR(s)
#define XSTR(s)  #s



// This defines built-in block "FileOut"

// We are in a C-like namespace of FileOut_
//
// symbols that are shared outside this file are prefixed with "FileOut_"
//
// make anything else that is file scope be static.

// We allocate a FileOut for each FileOut block loaded.
struct FileOut {

    char *filename; // filename, if used
    int fd;         // file descriptor
    bool weOpened;  // did we open the file?
};


static
void CleanUpFilename(struct FileOut *f) {

    DASSERT(f);

    if(f->filename) {
#ifdef DEBUG
        memset(f->filename, 0, strlen(f->filename));
#endif
        free(f->filename);
        f->filename = 0;
    }
}


static void CleanUpFd(struct FileOut *f) {

    DASSERT(f);

    if(f->weOpened) {
        DASSERT(f->fd > -1);
        close(f->fd);
        f->weOpened = false;
        f->fd = -1;
    }
}


static
char *InputMax_config(int argc, const char * const *argv,
        struct FileOut *f) {

    DASSERT(f);

    size_t inputMax = qsParseSizet(DEFAULT_INPUTMAX);

    if(inputMax < 1)
        inputMax = 1;

    qsSetInputMax(0, inputMax);

    return mprintf("InputMax %zu", inputMax);
}


static
char *Filename_config(int argc, const char * const *argv,
        struct FileOut *f) {

    DASSERT(f);

    if(argc < 2)
        // Kind of pointless of the user to do this?
        return 0;

    f->filename = strdup(argv[1]);
    ASSERT(f->filename, "strdup() failed");
    ASSERT(f->filename[0]);
    CleanUpFd(f);

    return mprintf("Filename %s", f->filename);
}


static
char *FileDescriptor_config(int argc, const char * const *argv,
        struct FileOut *f) {

    DASSERT(f);

    if(argc < 2)
        // Kind of pointless of the user to do this?
        return 0;

    CleanUpFilename(f);

    int fd = qsParseInt32t(-1);

    if(fd <= -1) return QS_CONFIG_FAIL;

    f->fd = fd;

    return mprintf("FileDescriptor %d", f->fd);
}



int FileOut_declare(void) {

#if 0
    {
        // TODO:
        // Houston, we have a problem: this little block of code should
        // make a valgrind memory leak test run fail, but it did not.
        // Shit!  Is it a bug in valgrind?  Oh well...
        char *x = malloc(32);
        *x = 3;
    }
#endif


    struct FileOut *f = calloc(1, sizeof(*f));
    ASSERT(f, "calloc(1,%zu) failed", sizeof(*f));

    // Defaults
    DASSERT(STDOUT_FILENO == 1);
    f->fd = STDOUT_FILENO; // Why the hell do they define this?

    qsSetUserData(f);

    // This block is a sink with a single input stream
    qsSetNumInputs(1, 1);

    qsSetInputMax(0/*port*/, DEFAULT_INPUTMAX);


    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            InputMax_config, "InputMax",
            "Bytes written per flow() call.",
            "InputMax BYTES",
            "InputMax " STR(DEFAULT_INPUTMAX));

    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            Filename_config, "Filename",
            "Set the filename to write to."
            " This will override configuration"
            " attribute \"FileDescriptor\"",
            "Filename FILENAME",
            "Filename ");

    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            FileDescriptor_config, "FileDescriptor",
            "Set the file descriptor number to write to."
            " This will override configuration"
            " attribute \"Filename\"",
            "FileDescriptor NUM",
            "FileDescriptor 1");

    return 0; // success
}


int FileOut_start(uint32_t numInputs, uint32_t numOutputs,
        struct FileOut *f) {

    DSPEW();

    DASSERT(f);
    // This is a sink block with one input port
    DASSERT(numInputs == 1);
    DASSERT(numOutputs == 0);

    
    if(f->filename && f->filename[0]) {
        f->fd = open(f->filename, O_WRONLY|O_CREAT|O_APPEND, 0666);
        if(f->fd < 0) {
            ERROR("open(\"%s\", O_WRONLY|O_CREAT|O_APPEND,"
                    "0666) failed", f->filename);
            return 1; // Fail this cycle
        }
        f->weOpened = true;
    }

    if(f->fd < 0) {
        ERROR("No file descriptor set");
        return 1; // Fail/bail for this cycle.
    }

    DSPEW("f->filename=\"%s\"  fd=%d", f->filename, f->fd);

    return 0;
}


int FileOut_flow(const void * const in[], const size_t inLens[],
        uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        struct FileOut *f) {

    DASSERT(f->fd >= 0);

    if(!(*inLens))
        // Got nothing to write out to the file.
        return 0;


    ssize_t ret = write(f->fd, *in, *inLens);

    if(ret <= 0) {
        if(ret < 0)
            WARN("write(%d,%p,%zu)=%zd failed", f->fd, *in, *inLens, ret);
        else
            // Returned 0.  I'm not sure what this means.
            WARN("write(%d,%p,%zu) returned 0", f->fd, *in, *inLens);
        return 1; // done for this flow cycle.
    }

    qsAdvanceInput(0, ret);

    return 0; // success
}


int FileOut_stop(uint32_t numInputs, uint32_t numOutputs,
        struct FileOut *f) {

    DSPEW();

    CleanUpFd(f);

    return 0;
}


int FileOut_undeclare(struct FileOut *f) {

    DASSERT(f);
    DSPEW();

    CleanUpFilename(f);
    CleanUpFd(f);

#ifdef DEBUG
    memset(f, 0, sizeof(*f));
#endif
    free(f);

    return 0;
}
