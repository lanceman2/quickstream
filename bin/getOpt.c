#include <string.h>

#include "../lib/debug.h"

#include "./quickstream.h"


int getOpt(int argc, const char * const *argv, int i,
        const struct opts *options/*array of options*/,
        const char **command) {

    DASSERT(i < argc);

    *command = "*";

    const char *str = argv[i];


    if(str == argv[i]) {
        // This is the start of an arg string

        if(*str != '-') {
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
                        *command = o->longOpt;
                        return o->shortOpt;
                    } else if(ll == strlen(str) && i < argc) {
                        // example: --long-opt ARG
                        //            ^
                        *command = o->longOpt;
                        return o->shortOpt;
                    }
                }
            }
            return '*'; // Not an option.
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
                ;
            else if(i+1 < argc && *argv[i+1] != '-')
                // example:  -acl foo
                //              ^
                ++i;
            else {
                // example:  -acl -foo
                //              ^ 
                if(i < argc)
                    ++i;
            }

            *command = o->longOpt;
            return o->shortOpt;
        }
    }

    return '*'; // Not an option.
}
