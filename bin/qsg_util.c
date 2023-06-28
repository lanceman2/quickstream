#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "../include/quickstream.h"

#include "quickstreamGUI.h"


// Splice 2 strings.
//
// Returns malloc() allocated string.
char *Splice2(const char *s1, const char *s2) {

    const size_t strLen = strlen(s1) + strlen(s2) + 1;
    char *text = malloc(strLen);
    ASSERT(text, "malloc(%zu) failed", strLen);
    text[0] = '\0';
    strcat(text, s1);
    strcat(text, s2);
    return text;
}


// Returned value must be free()ed.
//
char *GetParameterValueString(struct QsParameter *p) {

    char *ret = 0;
    size_t size = qsParameter_getSize(p);
    DASSERT(size);
    size_t tlen = 0;
    void *value = calloc(1,size);
    ASSERT(value, "calloc(1,%zu) failed", size);

    if(size != qsParameter_getValue(p, value, size)) {
        ret = strdup("");
        ASSERT(ret, "strdup() failed");
        goto finish;
    }

    switch(qsParameter_getValueType(p)) {

        case QsValueType_double:
        {
            DASSERT(size % sizeof(double) == 0);
            size_t n = size/sizeof(double);
            ret = malloc(tlen += n*10);
            ASSERT(ret, "malloc(%zu) failed", tlen);
            size_t wlen = tlen;
            char *s = ret;
            double *val = value;
            for(size_t i = 0; i < n;) {
                char *fmt;
                if(i)
                    fmt = ",%lg";
                else
                    fmt = "%lg";

                int w = snprintf(s, wlen, fmt, *val);
                ASSERT(w > 0);
                if(w >= wlen) {
                    size_t keepSLen = s - ret;
                    char *nt = realloc(ret, tlen += 32);
                    ASSERT(nt, "realloc(,%zu) failed", tlen);
                    wlen += 32;
                    // s is to where we written to that is good so far.
                    s = nt + keepSLen;
                    ret = nt;
                    continue;
                }
                s += w;
                ++val;
                wlen -= w;
                ++i;
            }
        }
            break;
        case QsValueType_uint64:
        {
            DASSERT(size % sizeof(uint64_t) == 0);
            size_t n = size/sizeof(uint64_t);
            ret = malloc(tlen += n*6);
            ASSERT(ret, "malloc(%zu) failed", tlen);
            size_t wlen = tlen;
            char *s = ret;
            uint64_t *val = value;
            for(size_t i = 0; i < n;) {
                char *fmt;
                if(i)
                    fmt = ",%" PRIu64;
                else
                    fmt = "%" PRIu64;

                int w = snprintf(s, wlen, fmt, *val);
                ASSERT(w > 0);
                if(w >= wlen) {
                    size_t keepSLen = s - ret;
                    char *nt = realloc(ret, tlen += 20);
                    wlen += 20;
                    ASSERT(nt, "realloc(,%zu) failed", tlen);
                    // s is to where we written to that is good so far.
                    s = nt + keepSLen;
                    ret = nt;
                    continue;
                }
                s += w;
                ++val;
                wlen -= w;
                ++i;
            }
        }
            break;
        case QsValueType_bool:
        {
            DASSERT(size % sizeof(bool) == 0);
            size_t n = size/sizeof(bool);
            ret = calloc(1, n*6); // true, false,
            ASSERT(ret, "calloc(1,%zu) failed", n*6);
            char *s = ret;
            bool *val = value;
            for(size_t i = 0; i < n; ++i) {
                char *fmt;
                if(i) {
                    *s = ',';
                    ++s;
                }

                size_t inc;

                if(*val) {
                    fmt = "true";
                    inc = 4;
                } else {
                    fmt = "false";
                    inc = 5;
                }

                strcpy(s, fmt);
                s += inc;
                ++val;
            }
        }
            break;
        case QsValueType_string32:
        {
            // TODO; We do not support arrays of string32 yet.
            // Changes in ../lib/parameter.c needed too which
            // will ASSERT() if the array length > 1.
            DASSERT(size == 32);
            //
            // We do not need to convert the value to a string since it is
            // one already.
            //
            ret = value;
            DASSERT(strlen(value) < 32);
            value = 0;
        }
            break;
        default:
            ASSERT(0, "MORE CODE HERE");
            break;
    }

finish:

    if(value) {
#ifdef DEBUG
        memset(value, 0, size);
#endif
        free(value);
    }

    return ret;
}
