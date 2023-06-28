#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>

#include "../lib/debug.h"

static
int count = 0;

static
void sighandler(int sig) {

    WARN("caught signal %d  count=%d", sig, ++count);
}



int main(void) {

    ASSERT(signal(SIGALRM, sighandler) == 0);
    // This is to fix a GCC compiler bug:
    // itimer_test.c:9:5: error: ‘count’ defined but not used [-Werror=unused-variable]
    // But you can clearly see it is used above.
    count = 0;

#if 0
    errno = 0;
    struct itimerval it = {  { 1/*seconds*/, 0/*microseconds*/ }, { 1,0 }};
    if(setitimer(ITIMER_REAL, &it, 0)) {
        ERROR("setitimer(ITIMER_REAL,,) failed");
        return -1; // fail
    }
#else
    errno = 0;
    struct itimerval it = {
        { 0/*seconds*/, 100000/*microseconds*/ }, { 0,100000 }};
    if(setitimer(ITIMER_REAL, &it, 0)) {
        ERROR("setitimer(ITIMER_REAL,,) failed");
        return -1; // fail
    }
#endif

    while(1) {
        errno = 0;
        pause();
        //ERROR();
    }

    return 0;
}
