#ifndef __mprintf_h__
#define __mprintf_h__


// A malloc() sprintf() mash.
//
// Good to generate the return value for a qsAddConfig() block callback
// function.  Makes a string in one line of code, instead of 5 or so.
//
// Really it's calloc() which is malloc() plus stuff zeroing and fancy
// multiplying.
//
// Allocate a sprintf string via vsnprintf() and the malloc family of
// functions.  The returned value must be freed.  It's like strdup() with
// printf formatting added.
//
// Returns a pointer to a malloced string.
//
static inline char *mprintf(const char *fmt, ...) {

    DASSERT(fmt);
    DASSERT(fmt[0]);

    va_list ap;

    // I would like the memory that is not written to on the end of
    // the string to be zeroed, so I use calloc() in place of
    // malloc().  Maybe in a non-debug build, malloc() would be a
    // little faster, but this code should not be in a tight loop,
    // and these should be small strings (< 2048 bytes).

    size_t len = strlen(fmt);
    char *str = calloc(1, len);
    ASSERT(str, "calloc(%zu) failed", len);
    va_start(ap, fmt);
    int nwr = vsnprintf(str, len, fmt, ap);
    va_end(ap);

    DASSERT(nwr > 0);

    //ERROR("str=\"%s\" len=%zu nwr=%d", str, len, nwr);

    if(len > nwr) return str;

    // We will remake the memory larger for the returned string.
#ifdef DEBUG
    memset(str, 0, len);
#endif
    free(str);

    len = nwr + 1;

    // Trying again.
    str = calloc(1, len);
    ASSERT(str, "calloc(%zu) failed", len);
    va_start(ap, fmt);
    nwr = vsnprintf(str, len, fmt, ap);
    va_end(ap);

    DASSERT(nwr > 0);

    // It should have worked on the second try given the first try
    // measured the memory (string length) that was needed.
    //DASSERT(len > nwr);
    DASSERT(len == nwr + 1);

    // To watch it in action:
    //ASSERT(0, "str=\"%s\" len=%zu nwr=%d", str, len, nwr);

    return str;
}

#endif
