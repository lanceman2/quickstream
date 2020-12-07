// Testing what system calls are made from sigsetjmp() and setjmp() and so
// on.
//
// example:
//
//        make
//        strace ./setjmp
//
//
// Results:
//
//    sigsetjmp()
//    calls system call rt_sigprocmask(SIG_SETMASK, [], NULL, 8) = 0
//
// and
//
//    setjmp()
//    makes no system calls
//

#include <errno.h>
#include <unistd.h>
#include <setjmp.h>

#include "../lib/debug.h"


int main(void) {

    //int count = 0;

    jmp_buf jmpEnv;


    for(int i=0; i<10000; ++i)
    if(setjmp(jmpEnv)) {
    //if(sigsetjmp(jmpEnv, 1)) {

        // We jumped into this point in the code.

        //++count;

        //DSPEW("count=%d", count);
    }

    //if(count < 10000)
        //siglongjmp(jmpEnv, 1);

    return 0;
}
