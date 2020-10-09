#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "../lib/Dictionary.h"
#include "../lib/debug.h"

static
void catchSegv(int sig) {
    fprintf(stderr, "\nCaught signal %d\n"
            "\nsleeping:  gdb -pid %u\n",
            sig, getpid());
    while(true) usleep(100000);
}


int main(int argc, const char **argv) {

    signal(SIGSEGV, catchSegv);

    struct QsDictionary *d = qsDictionaryCreate();

    const char *keys[] = {
        "app:", "app:",
        "app:stream", "app:stream",
        "app:stream:filter", "app:stream:filter",
        "app:stream:filter0", "app:stream:filter0",
        "app:stream:filter1", "app:stream:filter1",
        "f", "f",
        "g", "g",

        0, 0
    };



    for(const char **key = keys; *key; ++key) {
        const char *val = *(key + 1);
        ASSERT(qsDictionaryInsert(d, *key, val, 0) == 0);
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

    // Here's the thing we really needed to test:
    char *value = 0;
    struct QsDictionary *d0 = qsDictionaryFindDict(d, "app:", (void **) &value);
    ASSERT(d0);
    ASSERT(strcmp(value, "app:") == 0);
    struct QsDictionary *d1 = qsDictionaryFindDict(d0, "stream", (void **) &value);
    ASSERT(d1);
    ASSERT(strcmp(value, "app:stream") == 0);
    struct QsDictionary *d2 = qsDictionaryFindDict(d1, ":filter1", (void **) &value);
    ASSERT(d2);
    ASSERT(strcmp(value, "app:stream:filter1") == 0);


    qsDictionaryDestroy(d);

    fprintf(stderr, "\nTRY: %s | display\n\n", argv[0]);


    fprintf(stderr, "%s SUCCESS\n", argv[0]);

    return 0;
}
