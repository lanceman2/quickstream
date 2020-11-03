#include <string.h>
#include "getOpt.h"
#include "../lib/debug.h"


int getOpt(int argc, const char * const *argv, int *i,
        const struct opts *options/*array of options*/,
        const char **arg/*points to next possible arg*/) {

    DASSERT(*i < argc, "");

    const char *str = argv[*i];

    if(*arg)
        str = *arg; // The user left this unused.

    if(str == argv[*i]) {
        // This is the start of an arg string

        if(*str != '-') {
            *arg = str;
            return '*'; // Not an option.
        }

        ++str;
        if(*str == '-') {
            ++str;
            // str points long arg to the
            // example:  --long-opt
            //             ^
            for(const struct opts *o=options; o->longOpt; ++o) {

                size_t ll = strlen(o->longOpt);
                if(strncmp(o->longOpt, str, ll) == 0) {
                    if(str[ll] == '=') {
                        // example: --long-opt=ARG
                        //            ^
                        *arg = &str[ll+1];
                        return o->shortOpt;
                    } else if(ll == strlen(str) && *i < argc) {
                        // example: --long-opt ARG
                        //            ^
                        *arg = argv[++(*i)];
                        return o->shortOpt;
                    }
                }
            }
        }
        // Fall through case:
        //
        //  example:  -aclm
        //             ^
    }

    // arg points like for example:  -aclm
    //                                ^
    //                          or:   -aclm
    //                                   ^
    // So this must be a series of short options if they are options at
    // all.
    for(const struct opts *o=options; o->longOpt; ++o) {
        if(o->shortOpt == *str) {
            if(*(str+1))
                // example:  -acl
                //            ^
                *arg = str+1;
            else if(*i+1 < argc && *argv[*i+1] != '-')
                // example:  -acl foo
                //              ^
                *arg = argv[++(*i)];
            else {
                // example:  -acl -foo
                //              ^ 
                *arg = 0;
                if(*i < argc)
                    ++(*i);
            }

            return o->shortOpt;
        }
    }

    *arg = argv[*i];
    return '*'; // Not an option.
}
