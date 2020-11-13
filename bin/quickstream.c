#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include "../lib/debug.h"

#include "../include/quickstream/app.h"
#include "../include/quickstream/builder.h"

#include "getOpt.h"
#include "../lib/quickstream/misc/qsOptions.h"


// The spew level.
static int level = 1; // 0 == no spew, 1 == error, ...

// Current app:
static struct QsApp *app = 0;
// 
static uint32_t numApps = 0;
static struct QsApp **apps = 0;

static void CreateApp(void) {

    app = qsAppCreate();
    ASSERT(app);
    apps = realloc(apps, (++numApps)*sizeof(*apps));
    ASSERT(apps, "realloc(%p,%zu) failed", apps, numApps*sizeof(*apps));
    apps[numApps-1] = app;
}


// From quickstream_usage.c
// usage() does not return, it will exit.
extern void usage(int fd);


static void gdb_catcher(int signum) {

    // The action of ASSERT() depends on how this is compiled.
    ASSERT(0, "Caught signal %d\n", signum);
}


int main(int argc, const char * const *argv) {

    // Hang the program for debugging, if we segfault.
    signal(SIGSEGV, gdb_catcher);

    int i = 1;
    const char *arg = 0;


    qsSetSpewLevel(DEFAULT_SPEW_LEVEL);

    while(i < argc) {

        int c = getOpt(argc, argv, &i, qsOptions, &arg);

        switch(c) {

            // We put the exiting cases before the non-exiting cases in
            // this switch.  Some errors cause an exit in what would be
            // non-exiting cases.

            /////////////////////////////////////////////////////////////
            //               EXITING CASES                             //
            /////////////////////////////////////////////////////////////
            case -1:
            case '*':
                fprintf(stderr, "Bad option: %s\n\n", arg);
                // this will exit with error.
                return 1;
            case '?':
            case 'h':
                // The --help option get stdout and all the error
                // cases get stderr.
                usage(STDOUT_FILENO);
            case 'V': //--version
                printf("%s\n", QUICKSTREAM_VERSION);
                return 0;

            /////////////////////////////////////////////////////////////
            //           NON-EXITING CASES                             //
            /////////////////////////////////////////////////////////////

            case 'b': // --block FILENAME [BLOCK_NAME]
                if(!arg) {
                    fprintf(stderr, "--block with no FILENAME\n");
                    return 1;
                }
                {
                    const char *name = 0;
                    if((i+1)<argc && argv[i+1][0] != '-') {
                        ++i;
                        name = argv[i];
                    }
                    if(!app)
                        CreateApp();
                    struct QsBlock *b = qsAppBlockLoad(app, arg, name);
                    if(!b) {
                        fprintf(stderr, "Loading block %s FAILED\n",
                                arg);
                        return 1;
                    }
                }
                // next
                arg = 0;
                ++i;
                break;
            case 's': // --stream
                CreateApp();
                break;
            case 'S': // sleep SECONDS

                if(!arg) {
                    fprintf(stderr, "--sleep with no SECONDS\n");
                    return 1;
                }
                {
                    errno = 0;
                    char *endptr = 0;
                    double val = strtod(arg, &endptr);
                    if(endptr == arg || val <= 0.0) {
                        fprintf(stderr, "Bad --sleep option\n\n");
                        return 1;
                    }
                    if(level >= 4) // 4 = INFO
                        fprintf(stderr,"Sleeping %lg seconds\n", val);
                    usleep( (useconds_t) (val * 1000000.0));
                }
                // next
                arg = 0;
                ++i;
                break;
            case 'v':
                if(!arg) {
                    fprintf(stderr, "--verbose with no level\n");
                    return 1;
                }
                {
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

                default:
                // This should not happen, unless the options are not
                // coded correctly.  We are missing a case for this
                // char.
                ERROR("BAD CODE: Missing case for option character: "
                        "%c opt=%s arg=%s\n", c, argv[i-1], arg);
                fprintf(stderr, "unknown option character: %c\n", c);
                // this will exit with error.
                return 1;
        }
    }

    if(apps) {
        // a is a dummy QsApp pointer iterator.
        struct QsApp **a = apps + (numApps - 1);
        while(a >= apps) {
            qsAppDestroy(*a);
            --a;
        }
        // Free the array of pointers.
        free(apps);
    }

    return 0;
}
