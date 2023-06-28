#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// Returns a dlopen() handle.
static inline 
void *LoadDSOFromTmpFile(const char *path) {

    // This DSO (dynamic shared object) file is already loaded.  So we
    // must copy the DSO file to a temp file and load that.  Otherwise we
    // will have just the one plugin loaded, but referred to by two or
    // more blocks (filters), which is not what we want.  The temp file
    // will be automatically removed when the process exits.
    //

    INFO("DSO from %s was loaded before, loading a copy", path);
    char tmpFilename[32];
    strcpy(tmpFilename, "/tmp/qs_XXXXXX.so");
    int tmpFd = mkstemps(tmpFilename, 3);
    if(tmpFd < 0) {
        ERROR("mkstemp() failed");
        return 0;
    }
    // TODO: Bug: This DSPEW() seems to be adding an extra '\n' on the
    // end.
    DSPEW("made temporary file: %s", tmpFilename);
    int dso = open(path, O_RDONLY);
    if(dso < 0) {
        ERROR("open(\"%s\", O_RDONLY) failed", path);
        close(tmpFd);
        unlink(tmpFilename);
        return 0;
    }
    const size_t len = 1024;
    uint8_t buf[len];
    ssize_t rr = read(dso, buf, len);
    while(rr > 0) {
        ssize_t wr;
        size_t bw = 0;
        while(rr > 0) {
            wr = write(tmpFd, &buf[bw], rr);
            if(wr < 1) {
                ERROR("Failed to write to %s", tmpFilename);
                close(tmpFd);
                close(dso);
                unlink(tmpFilename);
                return 0;
            }
            rr -= wr;
            bw += wr;
        }
 
        rr = read(dso, buf, len);
    }
    close(tmpFd);
    close(dso);
    chmod(tmpFilename, 0700);

    void *handle = dlopen(tmpFilename, QS_MODULE_DLOPEN_FLAGS);
    //
    // This file is mapped to this process.  No other process will have
    // access to this temp file after the following unlink() call.
    //
    if(unlink(tmpFilename))
        // There is no big reason to fuss to much.
        WARN("unlink(\"%s\") failed", tmpFilename);

    if(!handle) {
        ERROR("dlopen(\"%s\",) failed: %s", tmpFilename, dlerror());
        return 0;
    }

    return handle;
}
