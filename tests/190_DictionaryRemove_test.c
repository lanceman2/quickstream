// Testing qsDictionaryRemove()

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <inttypes.h>
#include "../lib/Dictionary.h"
#include "../lib/debug.h"


static
void catchSegv(int sig) {
    fprintf(stderr, "\nCaught signal %d\n"
            "\nsleeping:  gdb -pid %u\n",
            sig, getpid());
    while(true) usleep(100000);
}



int main(int argc, char **argv) {

    signal(SIGSEGV, catchSegv);

    struct QsDictionary *d;

    d = qsDictionaryCreate();

    ASSERT(d);

    const char *keys[] = {
        "a", "a",
        "ab", "ab",
        "accc", "accc",
        "abccc", "abccc",
        "abcccc", "abcccc",
        "bccc", "bccc",
        "cccc", "cccc",
        "ccccc", "ccccc",
        "cdccc", "cdccc",
        "cdccc1", "cdccc1",
        "cdccc11", "cdccc11",
        "cdccc111", "cdccc111",
        "ceccc", "ceccc",
        "cdcccc", "cdcccc",

        0, 0
    };

    for(const char **key = keys; *key; ++key) {
        const char *val = *(key + 1);
        fprintf(stderr, "key=\"%s\", value=\"%s\"\n", *key, val);
        qsDictionaryInsert(d, *key, val, 0);
        ++key;
    }

#if 1
    ASSERT(qsDictionaryRemove(d, "cccc") == 0);
    ASSERT(qsDictionaryRemove(d, "ccccc") == 0);
    ASSERT(qsDictionaryRemove(d, "ceccc") == 0);
    ASSERT(qsDictionaryRemove(d, "ceccc") == 1);


    ASSERT(qsDictionaryRemove(d, "ab") == 0);
    ASSERT(qsDictionaryRemove(d, "ab") == 1);
    ASSERT(qsDictionaryRemove(d, "cdccc") == 0);
    ASSERT(qsDictionaryRemove(d, "cdccc11") == 0);

#endif


    qsDictionaryPrintDot(d, stdout);


    if((isatty(1)))
        fprintf(stderr, "\nTry running: %s | display\n\n", argv[0]);

    qsDictionaryDestroy(d);

    fprintf(stderr, "SUCCESS\n");

    return 0;
}
