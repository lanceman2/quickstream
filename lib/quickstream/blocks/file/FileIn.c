// This is the source code to a built-in block the is compiled into
// lib/libquickstream.so
//
// quickstream built-in block that is a source that reads a file using
// system call read(2).  This does not use the standard buffered file
// stream.  The user may set the file descriptor or a file path to
// open(2).

// TODO: Add a non-blocking read version/option that uses the qs
// epoll_wait(2) thread.

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "../../../debug.h"

#include "../../../../include/quickstream.h"

#define DEFAULT_OUTPUTMAX 2048
#define STR(s)   XSTR(s)
#define XSTR(s)  #s


// This defines built-in block "FileIn"

// We are in a C-like namespace of FileIn_
//
// symbols that are shared outside this file are prefixed with "FileIn_"
//
// make anything else that is file scope be static.

// We allocate a FileIn for each FileIn block loaded.
struct FileIn {

    char *filename;  // filename, if used
    int fd;          // file descriptor
    bool weOpened;   // did we open the file
};


static
void CleanUpFilename(struct FileIn *f) {

    DASSERT(f);

    if(f->filename) {
#ifdef DEBUG
        memset(f->filename, 0, strlen(f->filename));
#endif
        free(f->filename);
        f->filename = 0;
    }
}


static void CleanUpFd(struct FileIn *f) {

    DASSERT(f);

    if(f->weOpened) {
        DASSERT(f->fd > -1);
        close(f->fd);
        f->weOpened = false;
        f->fd = -1;
    }
}


static
char *OutputMax_config(int argc, const char * const *argv,
        struct FileIn *f) {

    DASSERT(f);

    size_t outputMax = qsParseSizet(DEFAULT_OUTPUTMAX);

    if(outputMax < 1)
        outputMax = 1;
 
    qsSetOutputMax(0, outputMax);

    return 0;
}


static
char *Filename_config(int argc, const char * const *argv,
        struct FileIn *f) {

    DASSERT(f);

    if(argc < 2)
        // Kind of pointless of the user to do this?
        return 0;

    f->filename = strdup(argv[1]);
    ASSERT(f->filename, "strdup() failed");
    ASSERT(f->filename[0]);
    CleanUpFd(f);

    return 0;
}


static
char *FileDescriptor_config(int argc, const char * const *argv,
        struct FileIn *f) {

    DASSERT(f);

    if(argc < 2)
        // Kind of pointless of the user to do this?
        return 0;

    CleanUpFilename(f);

    int fd = qsParseInt32t(-1);

    if(fd > -1)
        f->fd = fd;

    return 0; // success
}



int FileIn_declare(void) {

    struct FileIn *f = calloc(1, sizeof(*f));
    ASSERT(f, "calloc(1,%zu) failed", sizeof(*f));


    // Defaults
    DASSERT(STDIN_FILENO == 0);
    f->fd = STDIN_FILENO; // Why the hell do they define this?

    qsSetUserData(f);

    // This block is a source with a single output stream
    qsSetNumOutputs(1, 1);

    qsSetOutputMax(0/*port*/, DEFAULT_OUTPUTMAX);


    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            OutputMax_config, "OutputMax",
            "Bytes written per flow() call.",
            "OutputMax BYTES",
            "OutputMax " STR(DEFAULT_OUTPUTMAX));

    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            Filename_config, "Filename",
            "Set the filename to read to."
            " This will override configuration"
            " attribute \"FileDescriptor\"",
            "Filename FILENAME",
            "Filename"/*Not Set by default*/);

    qsAddConfig(
            (char *(*)(int, const char * const *, void *))
            FileDescriptor_config, "FileDescriptor",
            "Set the file descriptor number to read from."
            " This will override configuration"
            " attribute \"Filename\"",
            "FileDescriptor NUM",
            "FileDescriptor 0");

    return 0; // success
}


int FileIn_start(uint32_t numInputs, uint32_t numOutputs,
        struct FileIn *f) {

    DSPEW();

    DASSERT(f);
    // This is a source block with one output port
    DASSERT(numInputs == 0);
    DASSERT(numOutputs == 1);


    if(f->filename && f->filename[0]) {
        f->fd = open(f->filename, O_RDONLY);
        if(f->fd < 0) {
            ERROR("open(\"%s\", O_RDONLY) failed", f->filename);
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


int FileIn_flow(const void * const in[], const size_t inLens[],
        uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        struct FileIn *f) {

    DASSERT(f->fd >= 0);

    if(!(*outLens))
        // Got no room to write output to.
        return 0;

    ssize_t ret = read(f->fd, *out, *outLens);

    if(ret <= 0) {
        if(ret < 0)
            WARN("read(%d,%p,%zu)=%zd failed", f->fd, *out, *outLens, ret);
        else
            // Returned 0.  End of file, so I guess.
            INFO("read(%d,%p,%zu) returned 0", f->fd, *out, *outLens);
        return 1; // done for this flow cycle.
    }

    qsAdvanceOutput(0, ret);

    return 0; // success
}


int FileIn_stop(uint32_t numInputs, uint32_t numOutputs,
        struct FileIn *f) {

    DSPEW();

    CleanUpFd(f);

    return 0;
}


int FileIn_undeclare(struct FileIn *f) {

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
