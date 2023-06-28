#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../lib/debug.h"


// What we want:
//
// 1. pseudo-random data.
// 2. ASCII format so we can look at it.
// 3. Generated on the fly and not from a file.
// 4. Does not have to be high quality.
// 5. Works with the program diff so adding '\n' is nice.
// 6. It's not the most efficient, but much faster than using files.
//

//////////////////////////////////////////////////////////////////////
//                 CONFIGURATION
//////////////////////////////////////////////////////////////////////
#define NEWLINE_LEN (100) // Add a newline at this length

// Got seeds from running: sha512sum ANYFILE.
#define SEED_0 (0x5421aa1773593c60)
#define SEED_1 (0x3e13d82d9622b0c3)
#define SEED_2 (0xf0e05727881623bd)

static const char *charSet = "0123456789abcdef";

// This is the default total output length to all output channels
// for a given stream run.
#define DEFAULT_TOTAL_LENGTH   ((size_t) 8000)
#define DEFAULT_SEED_OFFSET    ((uint32_t) 13)
#define MAX_OUTPUTS  (9) // This MAX_OUTPUTS is pretty arbitrary.

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////



// Make a somewhat random ascii character string sequence using
// lrand48_r()

struct RandomString {
    struct drand48_data buffer;
    long int remainder;
    int remainderIndex;
    int newlineLen;
    int newlineIndex;
    bool init; // flag: is this struct initialized.
};


// Writes to string at *val_in.
//
// Returns the remaining length.
//
// I wish C had pass by reference, like C++ has.  All the arguments would
// be passed by reference, and this would be more truly inline code.
static inline
size_t GetNextStrings(struct RandomString *r,
        int *index, long int field, char **val_in, size_t len) {

    int i = *index;
    char *val = *val_in;

    while(i!=-1 && len) {

        ++r->newlineIndex;

        if(r->newlineLen && r->newlineIndex == r->newlineLen) {
            *val++ = '\n';
            --len;
            r->newlineIndex = 0;
            continue;
        }

        // Get the next 4 bits as a number, 2^4 = 16
        int n = (field & (15 << i*4)) >> i*4;
        *val++ = charSet[n]; // one byte at a time.

        --len;
        --i;
    }

    *val_in = val;
    *index = i;
    return len;
}


// seed can be like 0, 1, 2, ... which can be the channel/port number.
//
static void randomString_init(struct RandomString *r, uint32_t seed) {

    // We use the same initialization for all, so the sequence is always
    // the same.
    r->init = true;

    // This is the seed for the random number generator.  Change this
    // to change the sequence.  Got random data from the program
    // sha512sum.  I have no idea what these numbers do, but random
    // initial values seemed reasonable.
    uint64_t x[3] = {
        0x5421aa1773593c60,
        0x3e13d82d9622b0c3,
        0xf0e05727881623bd
    };

    // Note: We do not want to have seed effect the other two x[] numbers
    // because than they'd be related and we may see patterns in the
    // generated data because of that; as I did.
    x[0] += seed;


    ASSERT(NEWLINE_LEN >= 2, "NEWLINE_LEN must be greater than 1");

    r->remainderIndex = -1;

    ASSERT(sizeof(r->buffer) == sizeof(x));
    memcpy(&r->buffer, &x, sizeof(r->buffer));
    r->newlineLen = NEWLINE_LEN;
    r->newlineIndex = 0;
}


// len  -- is the length in bytes requested
//
// The sequence generated is not dependent on each len value, just the
// total count requested from the first call.
//
static inline
char *randomString_get(struct RandomString *r, size_t len, char *retVal) {

    ASSERT(r->init);

    char *val = retVal;

    while(len) {

        if(r->remainderIndex !=-1) {
            // Use up the reminder first.
            len = GetNextStrings(r, &r->remainderIndex, r->remainder, &val, len);
            if(len == 0) break;
        }

        long int result;
        lrand48_r(&r->buffer, &result);

        int i=7;
        len = GetNextStrings(r, &i, result, &val, len);

        if(len == 0 && i!=-1) {
            r->remainderIndex = i;
            r->remainder = result;
        }
    }

    return retVal;
}
