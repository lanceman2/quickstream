#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

#include "../lib/Dictionary.h"
#include "../lib/debug.h"

static
void catchSegv(int sig) {
    fprintf(stderr, "\nCaught signal %d\n"
            "\nsleeping:  gdb -pid %u\n",
            sig, getpid());
    while(true) usleep(100000);
}

static size_t num2 = 0;
static size_t num_checkstop = 17;

static struct QsDictionary *d;


int callback(const char *key, void *value, void *userData) {

    ++num2;

    fprintf(stderr, " --(%zu)-- key=\"%s\" value=\"%s\"\n",
            num2,
            key,
            (const char *) value);

    ASSERT(value == qsDictionaryFind(d, key));

    ASSERT(num2 <= num_checkstop, "This callback did not stop as requested");

    if(num2 == num_checkstop) {
        // Testing that this callback is stopped if we return non-zero.
        //
        // reset num2 counter.
        num2 = 0;

        return 1; // stop calling this.
    }

    return 0;
}




int callback2(const char *key, void *value, void *userData) {

    ++num2;

    fprintf(stderr, " (%zu) key=\"%s\" value=\"%s\"\n",
            num2,
            key,
            (const char *) value);

    ASSERT(value == qsDictionaryFind(d, key));
    return 0;
}


int main(int argc, const char **argv) {

    signal(SIGSEGV, catchSegv);

    d = qsDictionaryCreate();

    const char *keys[] = {
        "kea", "kea",
        "k1ea  ", "k1ea  ",
        "keb", "keb",
        "k eb", "k eb",
        "ke b", "ke b",
        " keb", " keb",
        "healo", "healo",
        "heal", "heal",
        "hea", "hea",
        "~kea", "kea",
        "~k1ea  ", "k1ea  ",
        "~keb", "keb",
        "~k eb", "k eb",
        "~ke b", "ke b",
        "~ keb", " keb",
        "~healo", "healo",
        "~heal", "heal",
        "~hea~", "hea eat me",

        " 0123", "0123",
        " 01234", "0123",
        " 0123456", "0123",
        " 012345", "0123",
        " 123", "0123",
 
        "0123", "0123",
        "01234", "0123",
        "0123456", "0123",
        "012345", "0123",
        "123", "0123",
        "1111111", "111111",
        "11111", "111111",
        "1111x11", "111111",
        "1111x111", "111111",
        "1111z11", "111111",
        "1111g11", "111111",
        "1111", "111111",
        "x00123", "0123",
        "w00123", "0123",
        "00123", "0123",
        "0", "0",
        "0124", "0123",
        "099", "099",
        "foo", "fooVal",
        "bar", "barVal",
        "boo", "boo",
        "Bar", "Bar",
        "Star start3", "Star:start3Val",
        "Star start", "poo",
        "Star star", "poo",
        "dog", "poo",
        "cat", "poo",
        "00", "0",
        "catdog", "poo   from catdog",
        "hay stack", "needle",
        "h", "h",
        "aa", "aa",
        "ab", "ab",
        "01", "01",
        "02", "02",
        "012", "012",
        "a", "a",
        "b", "b",
        "c", "c",
        "d", "d",
        "e", "e",
        "f", "f",
        "x40123", "0123",
        "x01234", "0123",
        "x0123456", "0123",
        "x012345", "0123",
        "x123", "0123",
        "x1111111", "111111",
        "x11111", "111111",
        "x1111x11", "111111",
        "x1111x111", "111111",
        "x1111z11", "111111",
        "x1111g11", "111111",
        "x1111", "111111",
        "xxr00123", "0123",
        "xwr00123", "0123",
        "xx00123", "0123",
        "x0", "0",
        "x0124", "0123",
        "x099", "099",
        "xfoo", "fooVal",
        "xbar", "barVal",
        "xboo", "boo",
        "xBar", "Bar",
        "xStar start3", "Star:start3Val",
        "xStar start", "poo",
        "xStar star", "poo",
        "xdog", "poo",
        "xcat", "poo",
        "x00", "0",
        "xcatdog", "poo   from catdog",
        "xhay stack", "needle",
        "xh", "h",
        "xaa", "aa",
        "xab", "ab",
        "x01", "01",
        "x02", "02",
        "x012", "012",
        "q", "q",

        0, 0
    };

    size_t num1 = 0;
    
    for(const char **key = keys; *key; ++key) {
        const char *val = *(key + 1);
        ASSERT(qsDictionaryInsert(d, *key, val, 0) == 0);
        ++num1;
        fprintf(stderr, "added %s, %s\n", *key, val);
        ++key;
    }

    qsDictionaryPrintDot(d, stdout);


    for(const char **key = keys; *key; ++key) {
        const char *stored = qsDictionaryFind(d, *key);
        const char *val = *(key + 1);
        ASSERT(val == stored, "key=\"%s\" val=\"%s\" != stored=\"%s\"",
                *key, val, stored);
        fprintf(stderr, "key=\"%s\", val=\"%s\"\n", *key, val);
        ++key;
    }

    qsDictionaryForEach(d, callback, 0);
    qsDictionaryForEach(d, callback2, 0);


    ASSERT(num1 == num2);

    qsDictionaryDestroy(d);

    fprintf(stderr, "%s SUCCESS\n", argv[0]);

    return 0;
}
