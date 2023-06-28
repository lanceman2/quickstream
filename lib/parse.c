#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../include/quickstream.h"

#include "debug.h"
#include "Dictionary.h"

#include "c-rbtree.h"
#include "name.h"
#include "threadPool.h"
#include "block.h"
#include "graph.h"
#include "job.h"
#include "config.h"

#include "parseBool.h"



static inline
bool
ParseSizet(const char *s, size_t *x) {

    char *end = 0;
    size_t val = strtoul(s, &end, 10);
    if(end == s) {
        WARN("cannot parse unsigned long from arg=\"%s\"", s);
        return false; // fail
    }
    *x = val;
    return true; // success
}


static inline
bool
ParseUint32t(const char *s, uint32_t *x) {

    char *end = 0;
    uint32_t val = strtoul(s, &end, 10);
    if(end == s) {
        WARN("cannot parse unsigned long from arg=\"%s\"", s);
        return false; // fail
    }
    // This will convert.
    *x = val;
    return true; // success
}

static inline
bool
ParseUint64t(const char *s, uint64_t *x) {

    char *end = 0;
    uint64_t val = strtoul(s, &end, 10);
    if(end == s) {
        WARN("cannot parse unsigned long from arg=\"%s\"", s);
        return false; // fail
    }
    // This will convert.
    *x = val;
    return true; // success
}


int32_t qsParseInt32t(int32_t defaultVal) {

    NotWorkerThread();

    struct QsModule *m = GetModule(CB_CONFIG, 0);

    // Does this have to be true???
    DASSERT(sizeof(unsigned long) == sizeof(size_t));

    char *end = 0;

    if(m->i >= m->argc) {
        WARN("cannot parse int32_t, out of arguments");
        return defaultVal;
    }

    int32_t val = strtol(m->argv[m->i], &end, 10);
    if(end == m->argv[m->i]) {
        ++m->i;
        WARN("cannot parse int32_t from arg=\"%s\"", end);
        return defaultVal;
    }

    ++m->i;

    return val;
}


size_t qsParseSizet(size_t defaultVal) {

    NotWorkerThread();

    struct QsModule *m = GetModule(CB_CONFIG, 0);

    // Does this have to be true???
    DASSERT(sizeof(unsigned long) == sizeof(size_t));


    char *end = 0;

    if(m->i >= m->argc) {
        WARN("cannot parse size_t, out of arguments");
        return defaultVal;
    }

    size_t val = strtoul(m->argv[m->i], &end, 10);
    if(end == m->argv[m->i]) {
        ++m->i;
        WARN("cannot parse size_t from arg=\"%s\"", end);
        return defaultVal;
    }

    ++m->i;

    return val;
}


// To skip ahead some number of arguments.
void qsParseAdvance(uint32_t inc) {

    NotWorkerThread();

    struct QsModule *m = GetModule(CB_CONFIG, 0);

    m->i += inc;
}


double qsParseDouble(double defaultVal) {

    NotWorkerThread();

    struct QsModule *m = GetModule(CB_CONFIG, 0);

    char *end = 0;

    if(m->i >= m->argc) {
        WARN("cannot parse double, out of arguments");
        return defaultVal;
    }

    double val = strtod(m->argv[m->i], &end);
    if(end == m->argv[m->i]) {
        ++m->i;
        WARN("cannot parse double from arg=\"%s\"", end);
        return defaultVal;
    }

    ++m->i;

    return val;
}


// This is almost the same as qsParseUint32Array().
void
qsParseSizetArray(size_t defaultVal, size_t *array, size_t len) {

    NotWorkerThread();

    struct QsModule *m = GetModule(CB_CONFIG, 0);

    // Does this have to be true???
    DASSERT(sizeof(unsigned long) == sizeof(size_t));

    if(m->i >= m->argc) {
        WARN("cannot parse size_t, out of arguments");
        for(int j=0; j<len; ++j)
            array[j] = defaultVal;
        return;
    }

    int i = m->i;
    int j = 0;
    int argc = m->argc;

    while(j<len && i<argc) {
        if(ParseSizet(m->argv[i], array + j)) {
            ++i;
            ++j;
        } else
            break;
    }
    m->i = i;


    if(j == len) return; // total success, got all values.

    if(j) {
        // We got at least one value at array[j].

        // Reuse dummy i.
        i = j-1;
        for(; j<len; ++j)
            // repeat the last good value to the rest.
            array[j] = array[i];
        return;
    }

    // We got no good values parsed:
    for(i=0; i<len; ++i)
        array[i] = defaultVal;
}


// This is almost the same as qsParseSizetArray().
void
qsParseUint32tArray(uint32_t defaultVal, uint32_t *array, size_t len) {

    NotWorkerThread();

    struct QsModule *m = GetModule(CB_CONFIG, 0);

    if(m->i >= m->argc) {
        WARN("cannot parse uint32_t, out of arguments");
        for(int j=0; j<len; ++j)
            array[j] = defaultVal;
        return;
    }

    int i = m->i;
    int j = 0;
    int argc = m->argc;

    while(j<len && i<argc) {
        if(ParseUint32t(m->argv[i], array + j)) {
            ++i;
            ++j;
        } else
            break;
    }
    m->i = i;

    if(j == len) return; // total success, got all values.

    if(j) {
        // We got at least one value at array[j].

        // Reuse dummy i.
        i = j-1;
        for(; j<len; ++j)
            // repeat the last good value to the rest.
            array[j] = array[i];
        return;
    }

    // We got no good values parsed:
    for(i=0; i<len; ++i)
        array[i] = defaultVal;
}


// This is almost the same as qsParseSizetArray().
void
qsParseUint64tArray(uint64_t defaultVal, uint64_t *array, size_t len) {

    NotWorkerThread();

    struct QsModule *m = GetModule(CB_CONFIG, 0);

    if(m->i >= m->argc) {
        WARN("cannot parse uint64_t, out of arguments");
        for(int j=0; j<len; ++j)
            array[j] = defaultVal;
        return;
    }

    int i = m->i;
    int j = 0;
    int argc = m->argc;

    while(j<len && i<argc) {
        if(ParseUint64t(m->argv[i], array + j)) {
            ++i;
            ++j;
        } else
            break;
    }
    m->i = i;


    if(j == len) return; // total success, got all values.

    if(j) {
        // We got at least one value at array[j].

        // Reuse dummy i.
        i = j-1;
        for(; j<len; ++j)
            // repeat the last good value to the rest.
            array[j] = array[i];
        return;
    }

    // We got no good values parsed:
    for(i=0; i<len; ++i)
        array[i] = defaultVal;
}


static inline bool
ParseDouble(const char *s, double *x) {

    char *end = 0;
    *x = strtod(s, &end);
    if(end == s) {
        WARN("Failed to parse double from \"%s\"", s);
        return false;
    }
    return true;
}


void
qsParseBoolArray(bool defaultVal, bool *array, size_t len) {

    NotWorkerThread();

    struct QsModule *m = GetModule(CB_CONFIG, 0);

    int i = m->i;
    int j = 0;
    int argc = m->argc;

    while(j<len && i<argc) {
        array[j] = qsParseBool(m->argv[i]);
        ++i;
        ++j;
    }
    m->i = i;

    if(j == len) return; // total success, got all values.

    if(j) {
        // We got at least one value at array[j].
        //
        // Reuse dummy i.
        i = j-1;
        for(; j<len; ++j)
            // repeat the last good value to the rest.
            array[j] = array[i];
        return;
    }

    // We got no values parsed:
    for(i=0; i<len; ++i)
        array[i] = defaultVal;
}


void
qsParseDoubleArray(double defaultVal, double *array, size_t len) {

    NotWorkerThread();

    struct QsModule *m = GetModule(CB_CONFIG, 0);

    if(m->i >= m->argc) {
        WARN("cannot parse double, out of arguments");
        for(int j=0; j<len; ++j)
            array[j] = defaultVal;
        return;
    }

    int i = m->i;
    int j = 0;
    int argc = m->argc;

    while(j<len && i<argc) {
        if(ParseDouble(m->argv[i], array + j)) {
            ++i;
            ++j;
        } else
            break;
    }
    m->i = i;

    if(j == len) return; // total success, got all values.

    if(j) {
        // We got at least one value at array[j].

        // Reuse dummy i.
        i = j-1;
        for(; j<len; ++j)
            // repeat the last good value to the rest.
            array[j] = array[i];
        return;
    }

    // We got no good values parsed:
    for(i=0; i<len; ++i)
        array[i] = defaultVal;
}
