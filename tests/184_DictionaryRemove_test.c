// Testing qsDictionaryRemove()
//
// This was found to fail, and so this test was made to address a
// specific failure in libquickstream.so.  When the stream was
// removing filters in test ./007
//
// Tue May 19 2020 this passes (in the master code branch).

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
        "a_2", "a_2",
        "a_3", "a_3",
        0, 0
    };

    for(const char **key = keys; *key; ++key) {
        const char *val = *(key + 1);
        fprintf(stderr, "key=\"%s\", value=\"%s\"\n", *key, val);
        qsDictionaryInsert(d, *key, val, 0);
        ++key;
    }
    qsDictionaryPrintDot(d, stdout);


    ASSERT(qsDictionaryRemove(d, "a") == 0);
    ASSERT(qsDictionaryRemove(d, "a_2") == 0);
    ASSERT(qsDictionaryRemove(d, "a_3") == 0);

    if((isatty(1)))
        fprintf(stderr, "\nTry running: %s | display\n\n", argv[0]);

    qsDictionaryDestroy(d);

    fprintf(stderr, "SUCCESS\n");

    return 0;
}
