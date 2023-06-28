// Check every C file in this directory with:
//
//    cat *.c | ./_zerosInFile
//

#include <stdio.h>
#include <inttypes.h>


int main(void) {

    char c;
    uint64_t len = 0;
    uint64_t count = 0;

    // prompt the user what the hell we are doing.
    fprintf(stderr, "Reading stdin\n");

    while((c = getchar()) != EOF) {
        ++len;
        if(!c) {
            ++count;
            fprintf(stderr, "The file we read (%"
                    PRIu64 ") zero "
                    "in it at character number %"
                    PRIu64 "\n", count, len);
        }
    }

    if(!count)
        fprintf(stderr, "\nNo zeros found in "
                "this file from stdin, file length %"
                PRIu64 " bytes\n\n", len);
    else
        fprintf(stderr, "\nFound %" PRIu64
                " zeros from reading stdin\n\n",
                count);

    return 0;
}
