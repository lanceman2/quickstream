#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "./quickstream.h"

#include "../lib/quickstream/misc/qsOptions.h"

#include "../lib/debug.h"
#include "../include/quickstream.h"



// The maximum number of words per command
#define MAX_LEN   (64)




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


#if 0
static inline
bool IsLineContinuation(const char *line, size_t len) {

    return (len >= 2 && line[len-2] == '\\' && line[len-1] == '\n') ||
            (len >= 1 && line[len-1] == '\\');
}
#endif





// 0 args
// --interpreter
//
// 1 arg
// --interpreter <FILENAME>
//
// 2 args
// --interpreter <FILENAME> <START_LINE_NO>
//
//
int RunInterpreter(int numArgs, const char * const *arg) {

    FILE *file = stdin;
    const char *filename = 0;
    // Line numbers start counting at 1 (not 0).  Yes even in vim.
    int startLineNum = 1;
    if(numArgs >= 1)
        filename = arg[0];
    else if(numArgs == 0)
        file = stdin;

    if(numArgs == 2) {
        char *end = 0;
        startLineNum = strtol(arg[1], &end, 10);
        if(end == arg[1] || startLineNum < 1) {
            fprintf(stderr, "--interpreter failed"
                    " bad line number\"%s\"\n",
                    arg[1]);
            return 1;
        }
    INFO("skipping to line %d", startLineNum);

    }
    else if(numArgs > 2 || (filename && !filename[0])) {
        fprintf(stderr, "--interpreter failed"
                " wrong number option arguments\n");
        return 1;
    }

    if(filename) {
        file = fopen(filename, "r");
        if(!file) {
            fprintf(stderr, "--interpreter failed"
                    " to open file \"%s\"\n",
                    filename);
            return 1;
        }
    }

    char *line = 0;
    ssize_t nread;
    size_t len = 0;

    if(startLineNum != 1) {
        int skipNum = startLineNum - 1;
        while(skipNum &&
                (nread = getline(&line, &len, file)) != -1) {
            // We found that bash counts lines that end in "\\\n"
            // or "\\" or not counted until the next line.  So we do the
            // same.
            //if(IsLineContinuation(line, len))
            //    continue;
            --skipNum;
        }
        if(nread == -1) {
            // We have exhausted the file.
            // There were no commands and that's fine.
            if(line)
                free(line);
            return 0;
        }
    }

    char **argv = calloc(MAX_LEN, sizeof(*argv));
    ASSERT(argv, "calloc(%d,%zu) failed",
            MAX_LEN, sizeof(*argv));

    int lineCount = startLineNum - 1;
    exitStatus = 0;

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

             if(exitOnError) {
                exitStatus = 1;
                break;
             }
        }

        if(RunCommand(command, n, argv[0],
                    (const char * const *) argv)) {
            if(exitStatus)
                fprintf(stderr, "Interpreter command failed: %s  line %d\n",
                        argv[0], lineCount);
            if(exitOnError)
                break;
            else
                exitStatus = 0;
        }
    }

    free(line);
    free(argv);
    fclose(file);

    return exitStatus;
}
