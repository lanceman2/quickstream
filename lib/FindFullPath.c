#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>

#include "debug.h"
#include "FindFullPath.h"
#include "dir.h"


#define SEPCHR (':')



// On success returns malloc() allocated memory, and
// on failure returns 0.
//
// Returns full path that needs to be free()ed or 0.
static inline
char *CheckEnvFilename(const char *envPath, const char *filename) {

    DASSERT(envPath);
    DASSERT(envPath[0]);
    DASSERT(filename);
    DASSERT(filename[0]);

    // Return value if we get one:
    char *ret = 0;

    // We'll write to a copy of the environment variable as we
    // make tests of a constructed path string.
    char *env = strdup(envPath);
    // This is the longest possible path we will need to test.
    size_t len = strlen(envPath) + 1 + strlen(filename) + 1;
    char *testStr = calloc(1, len);

    // All just some dummy iterator pointers 
    char *str = env; // current start of a dir
    char *i = env; // Yet another dummy index.

    for(; *str ; str = i) {

        // Strip off leading SEPCHR in string str.
        for(; *str && *str == SEPCHR; ++str);
        if(!*str)
            // Nothing left to test.
            goto finish;
        // Bring i to start at str to search for an index to
        // terminate this chunk of the string.
        i = str;

        // At this point there is a string of interest to test because
        // *str is not '\0' or a SEPCHR.

        // Take i to the next SEPCHR in the string (or '\0').
        for(; *i && *i != SEPCHR; ++i);

        // Remove any '/' on the end.  It should not be there anyway, but
        // to may have came directly from a dumb end user.  If the dumb
        // ass put more than one '/' on the end we cover that too.  We
        // just making it a little more tidy for dumb users, but extra
        // repeated '/'s do not seem to keep UNIX file path names from
        // working.
        //
        // TODO: RANT: On the port to Windoz this will have some more
        // complexity.  Windoz added complexity to the whole file path
        // thing on windoz as compared to on UNIX.  On UNIX we have root
        // being "/" but on windoz we have C:\ which is three characters
        // not one; making code more complex with no added benefit for the
        // end user.  Have's a mathematical proof that windoz sucks:
        // 3 > 1  QED.   ;)
        //
        // i should be at least one index ahead of str; assuming I did not
        // fuck this up.  Tear off all trailing '/' characters except in
        // the case where str = "/"; setting the path as root may be
        // bad, but we have no pressing reason not to allow it.
        for(char *x = i - 1; x >= str && *x == DIRCHR; --x)
            *x = '\0';
        // So now if str was = "////" now it's ""
        // and if str was = "/foo/bar///" now it's "/foo/bar"

        // Looking ahead in the string.
        if(*i == SEPCHR) {
            *i = '\0';
            // go to the next char to test.
            ++i;
        } // else *i == '\0' already

        // Build the string to test
        snprintf(testStr, len, "%s%c%s", str, DIRCHR, filename);

        // Test the string.
        if(access(testStr, R_OK) == 0) {
            // realpath() returns malloc()ed memory which we in turn
            // return.
            ret = realpath(testStr, 0);
            // It would be odd if realpath() failed after access() did
            // not, but there is a race condition too.  If we ever see
            // this assert we'll have to edit this code.
            ASSERT(ret, "realpath(\"%s\",0) failed", filename);
        } else
            // Reset errno, because access() should have set it.
            errno = 0;
    }

finish:

    DZMEM(testStr, len);
    DZMEM(env, strlen(envPath));

    free(testStr);
    free(env);
    return ret;
}


// On success returns malloc() allocated memory, and
// on failure returns 0.
//
// Returns full path that needs to be free()ed or 0.
static inline
char *CheckFilename(const char *filename) {

    char *ret = 0;

    if(access(filename, R_OK) == 0) {
        ret = realpath(filename, 0);
        // It would be odd if realpath() failed after access() did not,
        // but there is a race condition too.
        ASSERT(ret, "realpath(\"%s\",0) failed", filename);
    } else
        // reset errno.
        errno = 0;

    return ret;
}


// Find the first matching full path to a file by searching
// with the many rules of this logic.
//
// The returned value will be from malloc()ed memory or 0 if none was
// found.  If a non-zero value was returned the user of this function must
// see that the returned value is free()ed.
//
// The found file is only checked by calling access(file, R_OK) with
// the R_OK flag.
//
// We figure that checking for write access or that the caller can do
// other operations with the file is beyond the scope of this function and
// in most cases is better left to the end user.
//
// So say we return a file, but the caller can't dl_open() that file, and
// there are other files that would match and be open-able with dl_open()
// than we are inclined to push that error to the user, and then let the
// end user see they have too many dam files with similar paths; and the
// file found is not usable.
//
// We thought about returning a list of files to be farther tested by the
// user, but we thought that it is simpler for the users to know that
// their end user is not able to use the first file found, as opposed to
// adding a lot of complexity to the task by giving the end user a more
// complex task in the event that it did not work.  It's a question of at
// what point the "process" do you need to fail.  You could go to an
// extreme and fail long after the file is loaded and processed half way
// through.  We just choose to fail way way before that at a most generic
// stage in the process and with simpler logic that is likely easier to
// debug.
//
//
// So after this file is found there will be two strings of interest:
//
//    The file "name" we searched for
//
//    The full path returned.
//
// The prefix and envPath, may be important, but are in the function user
// domain.  They may need to let end users know about there existence.
//
char *FindFullPath(const char *name,
        const char *prefix,
        const char *suffix,
        const char *envPath) {

    char *ret = 0;
    char *filename;
    size_t len;
    bool hasSuffix = false;

    const char *n, *s;

    DASSERT(name);
    DASSERT(name[0]);
#ifdef DEBUG
    // We do not want prefix to end in '/' unless it's just
    // prefix = "/".

    if(prefix && prefix[0] && prefix[1])
        ASSERT(*(prefix + strlen(prefix) - 1) != DIRCHR);
#endif

    if(!prefix)
        prefix = "";

    if(!suffix)
        suffix = "";

    len = strlen(prefix);
    if(envPath) {
        size_t l = strlen(envPath);
        if(len < l)
            len = l;
    }

    len += strlen(name) + strlen(suffix) + 2;
    filename = calloc(1, len);
    ASSERT(filename, "calloc(1,%zu) failed", len);

    // Make sure name has the suffix.  If not add it.

    // Start at last char.
    n = name + strlen(name) - 1;
    s = suffix + strlen(suffix) - 1;
    for(; n >= name && s >= suffix; --n, --s)
        if(*s != *n)
            break;
    if(s < suffix) {
        // The suffix was found at the end of name.
        hasSuffix = true;
        strcpy(filename, name);
    } else
        // We need to add the suffix.
        snprintf(filename, len, "%s%s", name, suffix);


    // 1. First check in envPath
    //
    // We want to check in this environment variable path thingy
    // first so that the end user may have more control over what
    // DSO files are used.  A user can override "core" block modules
    // by having the path in the environment variable finding the
    // DSO files that are not in "usual" places.  This gives the end
    // user the last say.

    if(envPath && envPath[0] &&
            (ret = CheckEnvFilename(envPath, filename)))
        goto finish;


    // 2. next check if name is a full path or in the current
    // directory.
    //
    // This case is an odd, but very convenient case for fly by the seat
    // of your pants developers.  If this makes a file conflict than case
    // 1, using an environment path variable, can fix a conflict.

    if((ret = CheckFilename(filename)))
        goto finish;
    if(filename[0] == DIRCHR)
        // That was a full path we just tested and failed to access.
        goto finish;


    // 3. Now check in prefix.
    //
    // This is the case where the DSOs are in the "core blocks" that are
    // parked in a sub-directory of the directory that libquickstream.so
    // is in.

    if(hasSuffix)
        // The suffix was found at the end of name.
        snprintf(filename, len, "%s%c%s", prefix, DIRCHR, name);
    else
        // We need to add the suffix.
        snprintf(filename, len, "%s%c%s%s", prefix, DIRCHR, name, suffix);

    if(access(filename, R_OK) == 0) {
        ret = realpath(filename, 0);
        // It would be odd if realpath() failed after access() did not.
        ASSERT(ret, "realpath(\"%s\",0) failed", filename);
    } else
        // reset errno.
        errno = 0;

finish:

#ifdef DEBUG
    if(filename[0])
        // We'll zero what we can.
        // Note: calloc() made filename be null terminated
        DZMEM(filename, strlen(filename));
#endif
    free(filename);

    return ret;
}
        
