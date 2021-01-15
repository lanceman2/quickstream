#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../lib/debug.h"

#include "quickstream.h"
#include "getOpt.h"
#include "../lib/quickstream/misc/qsOptions.h"



// The maximum number of words per command
#define MAX_LEN   (6)





static void
StripComments(char *line) {

    DASSERT(*line);

    for(char *s = line; *s; ++s)
        if(*s == '#' || *s == '\n') {
            *s = '\0';
            return;
        }
}

// We are sticking with ascii
//
static inline bool
IsInvalidChar(char c) {
    return c <= ' ' || c > '~';
}




int RunInterpreter(const char *filename) {

    char *line = NULL;
    ssize_t nread;
    size_t len = 0;
    FILE *file = stdin;

    if(filename && filename[0]) {
        file = fopen(filename, "r");
        if(!file) {
            fprintf(stderr, "--interpreter failed"
                    " to open file \"%s\"\n",
                    filename);
            return 1;
        }
    }

    char **argv = calloc(MAX_LEN, sizeof(*argv));
    ASSERT(argv, "calloc(%d,%zu) failed",
            MAX_LEN, sizeof(*argv));

    int lineCount = 0;

    while((nread = getline(&line, &len, file)) != -1) {

        int n = 0;
        StripComments(line);
        char *s = line;
        while(*s && n < MAX_LEN) {
            while(*s && IsInvalidChar(*s)) ++s;
            if(*s) argv[n++] = s;
            else break;
            while(!IsInvalidChar(*s)) ++s;
            if(!*s) break;
            *s++ = '\0';
        }

        ++lineCount;

        if(n == 0) continue;
#if 0
for(int i=0; i<n; ++i)
    printf("[%s]", argv[i]);
printf("\n");
#endif
        int command = 0;
        for(const struct opts *opt = qsOptions; opt->shortOpt; ++opt)
            if(strcmp(opt->longOpt, argv[0]) == 0 ||
                (opt->shortOpt == argv[0][0] && argv[0][1] == '\0')) {
                    command = opt->shortOpt;
                    break;
                }
        
        if(command == 0) {
            fprintf(stderr, "Unknown command: %s  line %d\n",
                    argv[0], lineCount);
            continue;
        }

        if(ProcessCommand(command, n-1, argv[0],
                    (const char * const *)&argv[1]))
            return 1; // fail
    }

    free(line);
    free(argv);
    fclose(file);
    return 0;
}
