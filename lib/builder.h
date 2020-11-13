
#define QS_BLOCK_PREFIX  "/lib/quickstream/blocks/"

#define DIR_CHAR '/'
#define DIR_STR  "/"


// Function used to find the path to blocks or other modules.
//
// Returns malloc()ed memory that must be free()ed allocated string that
// is a full path to a file, or the best guess of that.  The returned
// string starts with DIR_CHAR, '/' on linux.
//
// This ASSERTs if it fails.
//
// The returned string is not necessarily able to be opened with dlopen().
// The user must check the result from dlopen().
//
// Having env QS_BLOCK_PATH set effects this function.
//
// The suffix may or may not be at the end of the fileName string.
//
extern
char *GetPluginPath(const char *prefix,
        const char *fileName, const char *suffix);
