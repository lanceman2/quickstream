#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <stdatomic.h>

#ifndef SYS_gettid
#  error "SYS_gettid unavailable on this system"
#endif

#include "./debug.h"

#ifdef QS_EXPORT
// Just to check that quickstream.h is consistent with this file and
// debug.h:
#  include "../include/quickstream.h"
#endif


// This file is compiled into libquickstream.so
// When this file was compiled the spew level was:
#ifdef SPEW_LEVEL_DEBUG
#  define COMPILED_SPEW_LEVEL 5
#endif
#ifndef COMPILED_SPEW_LEVEL
#  ifdef SPEW_LEVEL_INFO
#    define COMPILED_SPEW_LEVEL 4
#  endif
#endif
#ifndef COMPILED_SPEW_LEVEL
#  ifdef SPEW_LEVEL_NOTICE
#    define COMPILED_SPEW_LEVEL 3
#  endif
#endif
#ifndef COMPILED_SPEW_LEVEL
#  ifdef SPEW_LEVEL_WARN
#    define COMPILED_SPEW_LEVEL 2
#  endif
#endif
#ifndef COMPILED_SPEW_LEVEL
#  ifdef SPEW_LEVEL_ERROR
#    define COMPILED_SPEW_LEVEL 1
#  endif
#endif
#ifndef COMPILED_SPEW_LEVEL
#  define COMPILED_SPEW_LEVEL 0
#endif

// So other code linking to this library may have higher spew levels, but
// the highest spew level from the code compiled with this value of
// SPEW_LEVEL_* will be CompiledSpewLevel.  The user of this compiled
// libquickstream.so will not be able to get more spew from code in
// libquickstream.so, but other code may have had higher spew levels if
// they use the spew macro functions: DSPEW(), INFO(), NOTICE(), WARN(),
// and ERROR().  That could be handy for making new filter modules.


// LEVEL may be debug, info, notice, warn, error, and
// off which translates to: 5, 4, 3, 2, 1, and 0 respectively.
//
// We need spewLevel to be thread safe so we make spewLevel atomic.  Note
// we only set it once and get it once in this file and it's not exposed
// to any other code.
static atomic_int spewLevel = COMPILED_SPEW_LEVEL;


int qsGetLibSpewLevel(void) {
    // Returns the compile time spew level.
    return COMPILED_SPEW_LEVEL;
}


// This is where the user of libquickstream.so can quiet down the code in the
// library, assuming that the library was not quiet already.
void qsSetSpewLevel(int level) {
    if(level > 5) level = 5;
    else if(level < 0) level = 0;
    spewLevel = level;
    //DSPEW("Spew level set to %d", level);
}



#define BUFLEN  1024

// in-lining _vspew() with inline may make debugging code a little harder.
//
static void _vspew(FILE *stream, int errn, const char *pre, const char *file,
        int line, const char *func, const char *fmt, va_list ap) {

    // TODO: What the hell good is buffer when stream is 0?

    // We try to buffer this "spew" so that prints do not get intermixed
    // with other prints in multi-threaded programs.
    char buffer[BUFLEN];
    int len;

    if(errn)
    {
        // TODO: GTK+3 keeps setting errno and I'm sick of seeing it.
        errno = 0;
        char estr[128];
        strerror_r(errn, estr, 128);
        // TODO: very Linux specific code here:
        len = snprintf(buffer, BUFLEN,
                "%s%s:%d:pid=%u:%zu %s():errno=%d:%s: ",
                pre, file, line,
                getpid(), syscall(SYS_gettid), func,
                errn, estr);
    } else
        len = snprintf(buffer, BUFLEN, "%s%s:%d:pid=%u:%zu %s(): ",
                pre, file, line,
                getpid(), syscall(SYS_gettid), func);

    if(len < 10 || len > BUFLEN - 40)
    {
        //
        // This should not happen.

        //
        // Try again without stack buffering
        //
        if(stream) {
            if(errn) {
                char estr[128];
                strerror_r(errn, estr, 128);
                fprintf(stream, "%s%s:%d:pid=%u:%zu %s():errno=%d:%s: ",
                    pre, file, line,
                    getpid(), syscall(SYS_gettid), func,
                    errn, estr);
            } else
                fprintf(stream, "%s%s:%d:pid=%u:%zu %s(): ",
                        pre, file, line,
                        getpid(), syscall(SYS_gettid), func);
        }

        int ret;
        ret = vsnprintf(&buffer[len], BUFLEN - len,  fmt, ap);
        if(ret > 0) len += ret;
        if(len + 1 < BUFLEN) {
            // Add newline to the end.
            buffer[len] = '\n';
            buffer[len+1] = '\0';
        }

        return;
    }

    int ret;
    ret = vsnprintf(&buffer[len], BUFLEN - len,  fmt, ap);
    if(ret > 0) len += ret;
    if(len + 1 < BUFLEN) {
        // Add newline to the end.
        buffer[len] = '\n';
        buffer[len+1] = '\0';
    }

    if(stream)
        fputs(buffer, stream);
}


void qs_spew(int levelIn, FILE *stream, int errn,
        const char *pre, const char *file,
        int line, const char *func,
        const char *fmt, ...)
{
    if(levelIn > spewLevel)
        // The spew level in is larger (more verbose) than one we let
        // spew.
        return;

    va_list ap;
    va_start(ap, fmt);
    _vspew(stream, errn, pre, file, line, func, fmt, ap);
    va_end(ap);
}



void (*qsAssertAction)(FILE *stream, const char *file,
        int lineNum, const char *func) = 0;



void qs_assertAction(FILE *stream, const char *file,
        int lineNum, const char *func)
{
    pid_t pid;
    pid = getpid();
    if(qsAssertAction)
        // We call the users assert action.  If it does not exit that's
        // okay, we'll just fall into the default behavior.
        qsAssertAction(stream, file, lineNum, func);
#ifdef ASSERT_ACTION_EXIT
    fprintf(stream, "Will exit due to error\n");
    exit(1); // atexit() calls are called
    // See `man 3 exit' and `man _exit'
#else // ASSERT_ACTION_SLEEP
    int i = 1; // User debugger controller, unset to effect running code.
    fprintf(stream, "  Consider running: \n\n  gdb -pid %u\n\n  "
        "pid=%u:%zu will now SLEEP ...\n", pid, pid, syscall(SYS_gettid));
    while(i) { sleep(1); }
#endif
}
