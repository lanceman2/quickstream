#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "debug.h"

#define DIR_CHAR ('/')

//
// Returned malloc() memory must be free()ed.
// or 0 if it's not found.
//
// example: category = "filters/"
//
// This returns a value if we can access it with the composed path.
//
//
static char *GetPluginPathFromEnv(const char *envs,
        const char *category,
        const char *name, const char *suffix)
{
    char *env = 0;
    size_t envLen = 0;
    char **envPaths = 0;
    {
        // TODO: We could find the longest directory path string in this
        // colon separated path list

        if(!(env = getenv(envs)))
            // move on to the next method.
            return env;

        envLen = strlen(env);

        DASSERT(*env, "env \"%s\" has zero length", envs);

        DSPEW("Got env: %s=%s", envs, env);

        // We make a copy of this colon separated path list.
        env = strdup(env);
        ASSERT(env, "strdup() failed");
        char *s = env;

        size_t i = 0;
        if(*env) ++i; // 1 path
        while(*s)
            if(*(s++) == ':') ++i; // + 1 path

        // Now "i" is the number of paths and we add a Null
        // string terminator.

        envPaths = (char **) malloc(sizeof(char *)*(i+1));
        ASSERT(envPaths, "malloc() failed");

        // Now go through the string again but find points in it
        // to be our paths in this (:) string-list copy.
        i = 0;
        envPaths[i++] = env;
        for(s=env; *s; ++s)
            if(*s == ':')
            {
                // get a pointer to the next string.
                envPaths[i++] = s+1;
                // terminate the path string.
                *s = '\0';
            }

        envPaths[i++] = 0; // Null terminate envPaths.

        // Now we have env and envPaths to free.

    }

    // len = strlen("$env" + '/' + category + name + ".so")
    // More than long enough.
    size_t suffixLen = strlen(suffix);

    const ssize_t len = envLen + strlen(category) +
            strlen(name) + suffixLen + 3 /* for '//' and '\0' */;

    // In case it's stupid long...
    DASSERT(len > 0 && len < 1024*1024);

    char *buf = (char *) malloc(len);
    ASSERT(buf, "malloc(%zu) failed", len);

    if(!strcmp(&name[strlen(name)-suffixLen], suffix))
        // The name already has suffix in it, so we will repurpose suffix.
        suffix = "";

    // So now envPaths[] is an array of strings that is Null terminated.
    for(char **path = envPaths; *path; ++path) {
        snprintf(buf, len, "%s/%s%s%s", *path, category, name, suffix);

        if(access(buf, R_OK) == 0) {
            // No memory leaks here:
            free(envPaths);
            free(env);
            // The user of this function must free buf.
            return buf; // success, we can access this file.
        } else
            errno = 0;
    }

    // No memory leaks here:
    free(envPaths);
    free(env);
    free(buf);
    return 0; // failed to access a file in "that path".
}


//
// A thread-safe path finder that looks at /proc/self which is the same as
// /proc/<PID>, which PID is the processes id number, like 2342.  Note,
// this is linux specific code using the linux /proc/ file system.
//
// example: category = "filters/"
//
// The returned pointer must be free()ed.
static inline
char *_GetPluginPath(const char *prefix, const char *category,
        const char *name, const char *suffix)
{
    DASSERT(name && strlen(name) >= 1);

    size_t suffixLen = strlen(suffix);
    DASSERT(suffix && suffixLen);

    if(name[0] == DIR_CHAR) {
        char *path;
        // We where given full path starting with '/'.
        size_t len = strlen(name);
        if(len > suffixLen && strcmp(&name[len-suffixLen], suffix) == 0)
            // There is a  (like ".so") suffix.
            path = strdup(name);
        else {
            // There is no (like ".so") suffix, so we add it.
            path = malloc(len + suffixLen + 1);
            snprintf(path, len + suffixLen + 1, "%s%s", name, suffix);
        }
        // This is the full path.
        return path;
    }

    DASSERT(category && strlen(category) >= 1);

    char *buf;

    if(strcmp(category, "filters/") == 0) {
        if((buf = GetPluginPathFromEnv("QS_MODULE_PATH", category,
                        name, suffix)))
            return buf;
        if((buf = GetPluginPathFromEnv("QS_FILTER_PATH", "",
                        name, suffix)))
            return buf;
    } else if(strcmp(category, "controllers/") == 0) {
        if((buf = GetPluginPathFromEnv("QS_MODULE_PATH", category,
                        name, suffix)))
            return buf;
        if((buf = GetPluginPathFromEnv("QS_CONTROLLER_PATH", "",
                        name, suffix)))
            return buf;
    } else if(strcmp(category, "run/") == 0) {
        if((buf = GetPluginPathFromEnv("QS_MODULE_PATH", category,
                        name, suffix)))
            return buf;
        if((buf = GetPluginPathFromEnv("QS_RUN_PATH", "",
                        name, suffix)))
            return buf;
    } else
        ASSERT(0, "category != \"filters/\" or \"controllers/\" Need "
                "to add category \"%s\"", category);


    // postLen = strlen("/lib/quickstream/plugins/" + category + name)
    const ssize_t postLen =
        strlen(prefix) + strlen(category) +
            /* for '/' and suffix (like ".so") and '\0' */
        strlen(name) + suffixLen + 2;
    DASSERT(postLen > 0 && postLen < 1024*1024);
    ssize_t bufLen = 128 + postLen;
    buf = malloc(bufLen);
    ASSERT(buf, "malloc(%zu) failed", bufLen);
    ssize_t rl = readlink("/proc/self/exe", buf, bufLen);
    ASSERT(rl > 0, "readlink(\"/proc/self/exe\",,) failed");
    while(rl + postLen >= bufLen)
    {
        DASSERT(bufLen < 1024*1024); // it should not get this large.
        buf = realloc(buf, bufLen += 128);
        ASSERT(buf, "realloc() failed");
        rl = readlink("/proc/self/exe", buf, bufLen);
        ASSERT(rl > 0, "readlink(\"/proc/self/exe\",,) failed");
    }

    buf[rl] = '\0';
    //
    // Now buf = path to program binary.
    //
    // Now strip off to after "/bin/qs_runner" (Linux path separator)
    // Or "/testing_crap/crap_runner"
    // by backing up two '/' chars.
    //
    --rl;
    while(rl > 5 && buf[rl] != '/') --rl; // one '/'
    ASSERT(buf[rl] == '/');
    --rl;
    while(rl > 5 && buf[rl] != '/') --rl; // two '/'
    ASSERT(buf[rl] == '/');
    buf[rl] = '\0'; // null terminate string

    // Now (for example):
    //
    //     buf = "/home/joe/installed/quickstream-1.0b4"
    // 
    //  or
    //     
    //     buf = "/home/joe/git/quickstream"
    //

    // Now just add the suffix to buf.
    //
    // If we counted chars correctly strcat() should be safe.

    DASSERT(strlen(buf) + strlen(prefix) +
            strlen(category) +
            strlen(name) + 1 < (size_t) bufLen);

    strcat(buf, prefix);
    strcat(buf, category);
    strcat(buf, name);

    // Reuse the bufLen variable.

    bufLen = strlen(buf);
    if(strcmp(&buf[bufLen-suffixLen], suffix))
        strcat(buf, suffix);

    return buf;
}


// This is the only exposed interface in this file.
//
char *GetPluginPath(const char *prefix, const char *category,
        const char *name, const char *suffix) {

    char *ret = _GetPluginPath(prefix, category, name, suffix);

    DASSERT(ret);

    // ret should be a malloc() allocated string a full path to a file, or
    // a best guess of that, but at this point we are not sure the file
    // exists and what type it is.   If dlopen() succeeds in opening it
    // then it's a DSO (dynamic shared object) that works with dlopen()
    // and that may be all we need to know.  If the python C API can run
    // the file as a script than it's a python script, or it fails and it
    // is not.  Simple enough.

    return ret;
}
