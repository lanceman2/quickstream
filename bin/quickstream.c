#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "../lib/debug.h"
#include "../include/quickstream/app.h"
#include "getOpt.h"
#include "../lib/quickstream/misc/qsOptions.h"

// From quickstream_usage.c
extern int usage(int fd);


static void gdb_catcher(int signum) {

    // The action of ASSERT() depends on how this is compiled.
    ASSERT(0, "Caught signal %d\n", signum);
}


int main(int argc, const char * const *argv) {

    // Hang the program for debugging, if we segfault.
    signal(SIGSEGV, gdb_catcher);

    int i = 1;
    const char *arg = 0;

    struct QsApp *app = 0;

    qsSetSpewLevel(DEFAULT_SPEW_LEVEL);

    while(i < argc) {

        int c = getOpt(argc, argv, &i, qsOptions, &arg);

        switch(c) {

            case -1:
            case '*':
                fprintf(stderr, "Bad option: %s\n\n", arg);
                return usage(STDERR_FILENO);

            case '?':
            case 'h':
                // The --help option get stdout and all the error
                // cases get stderr.
                return usage(STDOUT_FILENO);

            case 'V':
                printf("%s\n", QUICKSTREAM_VERSION);
                return 0;

            case 'v':
                if(!arg) {
                    fprintf(stderr, "--verbose with no level\n");
                    return usage(STDERR_FILENO);
                }
                {
                    int level;
                    // LEVEL maybe debug, info, notice, warn, error, and
                    // off which translates to: 5, 4, 3, 2, 1, and 0
                    // as this program (and not the API) define it.
                    if(*arg == 'd' || *arg == 'D' || *arg == '5')
                        // DEBUG 5
                        level = 5;
                    else if(*arg == 'i' || *arg == 'I' || *arg == '4')
                        // INFO 4
                        level = 4;
                    else if(*arg == 'n' || *arg == 'N' || *arg == '3')
                        // NOTICE 3
                        level = 3;
                    else if(*arg == 'w' || *arg == 'W' || *arg == '2')
                        // WARN 2
                        level = 2;
                    else if(*arg == 'e' || *arg == 'E' || *arg == '1')
                        // ERROR 1
                        level = 1;
                    else // none and anything else
                        // NONE 0 with a error string.
                        level = 0;

                    if(level >= 3/*notice*/)
                        fprintf(stderr, "quickstream spew level "
                                "set to %d\n"
                                "The highest libquickstream "
                                "spew level is %d\n",
                                level, qsGetLibSpewLevel());

                    qsSetSpewLevel(level);

                    // next
                    arg = 0;
                    ++i;
                    break;
                }
        }
    }

    app = qsAppCreate();

    qsAppDestroy(app);

    return 0;
}
