#include <inttypes.h>
#include <dlfcn.h>
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
#include "port.h"



struct MetaData {
    size_t size;
    void *userData;
};


static void DestroyMetaData(struct MetaData *m) {

    DASSERT(m);
    DASSERT(m->size);
    DASSERT(m->userData);

    DZMEM(m->userData, sizeof(m->size));
    free(m->userData);

    DZMEM(m, sizeof(*m));
    free(m);
}


// Returns a number from 0 to and including 63, the base 64 index
// of the char x.
//
// It's the inverse of const char *enc =
//    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
//    "abcdefghijklmnopqrstuvwxyz"
//    "0123456789+/";
//
// 
//
static inline uint32_t GetIndex(uint8_t x, int *error) {

    // Note: We do not need to look at the padding character '='
    // That should never makes it here.

    // First check for bad value.
    //
    if(x != '+' && x != '/' &&
            !('a' <= x && x <= 'z') &&
            !('A' <= x && x <= 'Z') &&
            !('0' <= x && x <= '9')) {
        *error = 1;
        ERROR("Bad base64 data at: '%c'", x);
        return 0;
    }

// We tested that both methods work:
#if 1
    // 43 = '+' is the smallest ascii
    // 122 = 'z' is the largest ascii
    // which gives us a span of 80 with 64 valid
    // values.
    //
    // 122 - 43 + 1 = 80
    const uint32_t index[122 - 43 + 1] = {
        //  +  ,  -  .   /
        // 43 44 45 46  47 
           62, 0, 0, 0, 63,
        //  0  1  2  3  4  5  6  7  8  9
        // 48 49 50 51 52 53 54 55 56 57 
           52,53,54,55,56,57,58,59,60,61,
        //  :  ;  <  =  >  ?  @
            0, 0, 0, 0, 0, 0, 0,
        //  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O  P  Q  R  S  T  U
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,
        //  V  W  X  Y  Z
           21,22,23,24,25,
        //  [  \  ]  ^  _  `
            0, 0, 0, 0, 0, 0,
        //  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o  p  q  r  s  t  u
           26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,
        //  v  w  x  y  z
           47,48,49,50,51
        };

    DASSERT(x >= 43);
    DASSERT(x <= 122);

    return index[x - 43];

#else
    // I expect this may be a little slower than the array method above.
    //
    if(x == '+')
        return 62;
    if(x == '/')
        return 63;
    if(x <= '9')
        // x = 0123456789
        return x - '0' + 52;
    if(x == '=')
        return 64;
    if(x <= 'Z')
        // x is ABCDEFGHIJKLMNOPQRSTUVWXYZ
        return x - 'A';
    //if(x <= 'z')
    // abcdefghijklmnopqrstuvwxyz
    return x - 'a' + 26;
#endif
}


static inline
int SetMetaData(struct QsDictionary *dict,
        const char *key, void *ptr, size_t size, bool ownPtr) {

    DASSERT(dict);
    DASSERT(key);
    DASSERT(ptr);
    DASSERT(size);

    // TODO: It seems a little wasteful to do two memory allocations when
    // we know only one is needed, but we do not want to assume how the
    // compiler pads memory between data structures.  How do you
    // dynamically get the size of a data structure that does not exist
    // (at least at this interface)?
    //
    // So instead of making bad assumptions, we just let the libc memory
    // allocator do the work, adding just a little bit of memory.
    //
    // Maybe there's a neat trick that I need to know.

    struct MetaData *m = calloc(1, sizeof(*m));
    ASSERT(m, "calloc(1,%zu) failed", sizeof(*m));

    struct QsDictionary *entry = 0;
    if(qsDictionaryInsert(dict, key, m, &entry) != 0) {
        free(m);
        ERROR("Meta data for key \"%s\" already exists", key);
        return 1;
    }

    m->size = size;

    if(ownPtr)
        m->userData = ptr;
    else {
        m->userData = calloc(1, size);
        ASSERT(m->userData, "calloc(1,%zu) failed", size);
        memcpy(m->userData, ptr, size);
    }

    DASSERT(entry);
    qsDictionarySetFreeValueOnDestroy(entry,
            (void (*)(void *)) DestroyMetaData);

    return 0;
}

#define QS_METADATA_PREFIX  "QS_"


static
void *qsBlock_getMetaData(const struct QsBlock *block,
        const char *key, size_t *size) {

    NotWorkerThread();

    DASSERT(block);
    ASSERT(block->type == QsBlockType_super);
    DASSERT(key);
    ASSERT(size);
    struct QsGraph *g = block->graph;
    DASSERT(g);

    CHECK(pthread_mutex_lock(&g->mutex));

    // This parses a very strict base64 encoded string.  If successful,
    // the input string has a length that is a multiple of 4 characters
    // with zero, one, or two '=' characters at the end. Like for example:
    //
    // "SGVsbG8AV29ybGQAIQAA" // or
    //
    // "SGVsbG8AV29ybGQhAAA=" // or
    //
    // "SGVsbG8AV29ybCEAAA=="
    //
    //
    // See: https://en.wikipedia.org/wiki/Base64
    //
    void *ret = 0;
    if(size) *size = 0;
    char *base64 = 0;
    int eerr = 0; // encode error

    {
        const size_t slen = strlen(QS_METADATA_PREFIX) + strlen(key) + 1;
        ASSERT(slen < 1000);
        char symbol[slen];
        strcpy(symbol, QS_METADATA_PREFIX);
        strcat(symbol, key);

        void *dlhandle = ((struct QsSuperBlock *)block)->module.dlhandle;
        DASSERT(dlhandle);

        uint8_t **sym = dlsym(dlhandle, symbol);
        if(sym)
            base64 = (char *) *sym;
        else
            ERROR("symbol \"%s\" not found", symbol);
    }

    if(!base64)
        goto finish;

    size_t len = strlen(base64);
    DASSERT(len >= 4);
    if(len % 4) {
        // Bad base64 encoded data, we require it be padded
        // to sets of 4 bytes.
        eerr = 1;
        goto finish;
    }

    size_t pads = 0;
    if(base64[len-1] == '=')
        ++pads;
    if(base64[len-2] == '=')
        ++pads;

    if(pads)
        // We process the padded 4 byte chunks
        // in a different loop.
        len -= 4;

    len /= 4;


    uint8_t *out;
    {
        // Find the output buffer size and allocate it.
        size_t size = 3*len;
        if(pads == 1)
            size += 2;
        else if(pads == 2)
            size += 1;

        out = malloc(size);
        ASSERT(out, "malloc(%zu) failed", size);
        ret = out;
    }

    uint8_t *in = (uint8_t *) base64;

    for(size_t i=0; i<len; ++i) {

        out[0] = (GetIndex(in[0], &eerr) << 2) +
            ((GetIndex(in[1], &eerr) & (03 << 4)) >> 4);
        out[1] = ((GetIndex(in[1], &eerr) & 0xF) << 4) +
             ((GetIndex(in[2], &eerr) & (0xF << 2)) >> 2);
        out[2] = ((GetIndex(in[2], &eerr) & 03) << 6) +
             (GetIndex(in[3], &eerr) & 077);
        out += 3;
        in += 4;
    }

    if(pads == 1) { // there is one '=' on the end

        out[0] = (GetIndex(in[0], &eerr) << 2) +
            ((GetIndex(in[1], &eerr) & (03 << 4)) >> 4);
        out[1] = ((GetIndex(in[1], &eerr) & 0xF) << 4) +
             ((GetIndex(in[2], &eerr) & (0xF << 2)) >> 2);

    } else if(pads) { // pads == 2 there is "==" on the end

        out[0] = (GetIndex(in[0], &eerr) << 2) +
            ((GetIndex(in[1], &eerr) & (03 << 4)) >> 4);
    }

    //printf("pads=%zu %s=\"%s\" = %s\n", pads,
    //        symbol, (char *) base64, (char *) ret);

finish:

    CHECK(pthread_mutex_unlock(&g->mutex));


    if(eerr) {
        ERROR("Bad base64 data =\"%s\"", base64);
        if(ret) {
            DZMEM(ret, len*3);
            free(ret);
            ret = 0;
        }
    } else if(ret && size) {
        // Return the size to the user.
        *size = len*3;
        if(pads)
            ++(*size);
        if(pads == 1)
            ++(*size);
    }

    return ret; // 0 or malloc()ed pointer returned.
}


static
int WriteMetaDataKeys(const char *key, struct MetaData *m,
            FILE *f) {

    DASSERT(f);
    DASSERT(m);
    DASSERT(m->size);

    fprintf(f,
"    \"%s\",\n",
        key);

    return 0; // continue
}


static
int WriteMetaDataEntry(const char *key, struct MetaData *m,
            FILE *f) {

    DASSERT(f);
    DASSERT(m);
    DASSERT(m->size);

    // base64 encode
    const char *enc =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    // https://en.wikipedia.org/wiki/Base64
    //
    // Encode 3 bytes at a time.
    uint32_t numTriplets = m->size / 3;
    // We'll handle the padding later.

    fprintf(f, "const char *" QS_METADATA_PREFIX "%s =\n    \"", key);

    uint8_t * data = m->userData;
    size_t i = 0;
    // We write 3N characters per line.
    const size_t N = 16;

    for(; i<numTriplets; ++i) {

        if(i && i % N == 0)
            // Put in some line breaks in the C code in this long
            // string.
            fprintf(f, "\"\n    \"");

        // 4 encodings per every 3 bytes of user data
        // data[0], data[1], data[2]:
        putc(enc[(data[0] & (077 << 2)) >> 2], f);
        putc(enc[((data[0] & 03) << 4) + ((data[1] & 0xF0) >> 4)], f);
        putc(enc[((data[1] & 0x0F) << 2) + ((data[2] & (03 << 6)) >> 6)],
                f);
        putc(enc[data[2] & 077], f);
        // Go to next 3 bytes.
        data += 3;
    }

    // Now padding with '=' if there is any.

    uint32_t extra = m->size % 3;

    if(extra == 1) {

        if(i && i % N == 0)
            // Put in some line breaks in the C code in this long
            // string.
            fprintf(f, "\"\n    \"");

        putc(enc[(data[0] & (077 << 2)) >> 2], f);
        putc(enc[((data[0] & 03) << 4)], f);
        putc('=', f);
        putc('=', f);

    } else if(extra == 2) {

        if(i && i % N == 0)
            // Put in some line breaks in the C code in this long
            // string.
            fprintf(f, "\"\n    \"");

        putc(enc[(data[0] & (077 << 2)) >> 2], f);
        putc(enc[((data[0] & 03) << 4) + ((data[1] & 0xF0) >> 4)], f);
        putc(enc[((data[1] & 0x0F) << 2)], f);
        putc('=', f);
    }
    // else there is no '=' padding.

    fprintf(f, "\";\n");


    return 0; // Keep going through the dictionary list.
}


#define QS_METADATA_KEYS  "qsMetaDataKeys"


void WriteMetaDataToSuperBlock(FILE *f, const struct QsGraph *g) {

    DASSERT(g);
    DASSERT(g->metaData);

    ASSERT(qsDictionaryForEach(g->metaData,
                (int (*) (const char *key, void *value,
                void *userData)) WriteMetaDataEntry, f));

    fprintf(f,
"\n"
"// A null terminated array of all metaData symbol suffixes above:\n"
"const char *" QS_METADATA_KEYS "[] = {\n"
            );
    ASSERT(qsDictionaryForEach(g->metaData,
                (int (*) (const char *key, void *value,
                void *userData)) WriteMetaDataKeys, f));

    fprintf(f,
"    0 /*0 null terminator*/\n"
"};\n"
            );
}


// Get the meta data from the super block and put into
// the graph meta data dictionary.
//
// Called after the super block is loaded and before the super block is
// flattened so all the children become all the blocks in the graph.
//
void qsGraph_createMetaDataDict(struct QsGraph *g,
            const struct QsBlock *b) {

    NotWorkerThread();

    DASSERT(g);
    DASSERT(b);
    DASSERT(b->type == QsBlockType_super);

    CHECK(pthread_mutex_lock(&g->mutex));

    struct QsModule *m = Get_Module((void *) b);

    const char **mdKeys = dlsym(m->dlhandle, QS_METADATA_KEYS);
    if(mdKeys) {

        ASSERT(!g->metaData);
        g->metaData = qsDictionaryCreate();
        ASSERT(g->metaData);

        // We have at least one metadata base64 string in this
        // super block, b.
        for(const char **key = mdKeys; *key; ++key) {

            size_t msize = 0;
            void *mData = qsBlock_getMetaData(b, *key, &msize);
            ASSERT(mData);
            ASSERT(msize);
            ASSERT(0 == SetMetaData(g->metaData, *key, mData, msize, true));
        }
    }

    CHECK(pthread_mutex_unlock(&g->mutex));
}


// Returned value must be free()ed if it's not 0.
//
// Get a malloc()ed copy of the decoded data.
//
void *qsGraph_getMetaData(const struct QsGraph *g,
        const char *key, size_t *ret_size) {

    NotWorkerThread();

    DASSERT(g);
    ASSERT(key); // bad user.

    CHECK(pthread_mutex_lock((void *) &g->mutex));

    void *ret = 0;
    if(ret_size) *ret_size = 0;


    if(!g->metaData) {
        CHECK(pthread_mutex_unlock((void *) &g->mutex));
        return ret;
    }

    struct MetaData *m = qsDictionaryFind(g->metaData, key);
    if(ret_size && m)
        *ret_size = m->size;


    if(m) {
        DASSERT(m->size);
        ret = malloc(m->size);
        ASSERT(ret, "malloc(%zu) failed", m->size);
        memcpy(ret, m->userData, m->size);
    }

    CHECK(pthread_mutex_unlock((void *) &g->mutex));

    return ret;
}


int qsGraph_setMetaData(struct QsGraph *g,
        const char *key, const void *ptr, size_t size) {

    NotWorkerThread();

    DASSERT(g);
    ASSERT(size);
    CHECK(pthread_mutex_lock(&g->mutex));

    if(!g->metaData) {
        g->metaData = qsDictionaryCreate();
        ASSERT(g->metaData);
    }

    int ret = SetMetaData(g->metaData, key, (void *) ptr, size, false);

    CHECK(pthread_mutex_unlock(&g->mutex));

    return ret;
}



void qsGraph_clearMetaData(struct QsGraph *g) {

    NotWorkerThread();
    DASSERT(g);

    CHECK(pthread_mutex_lock(&g->mutex));

    if(g->metaData) {
        qsDictionaryDestroy(g->metaData);
        g->metaData = 0;
    }

    CHECK(pthread_mutex_unlock(&g->mutex));
}

