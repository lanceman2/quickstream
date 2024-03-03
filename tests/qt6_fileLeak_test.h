
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>


#define FAIL(fmt, ...)\
    do {\
        fprintf(stderr,"\n" fmt " FAILED errno=%d\n\n", ##__VA_ARGS__,  errno);\
        exit(1);\
    } while(0)


static inline
void RunLsOnProcFd(const char *label) {

    pid_t parentPid = getpid();

    pid_t pid = fork();
    if(pid == -1)
        FAIL("fork()");
    
    if(pid) {
        // I'm the parent and I'll wait for the child
        // to run "ls".
        if(pid != waitpid(pid, 0, 0))
            FAIL("waitpid()");
        printf("\n");
        return;
    }

    // I'm the child that will run "ls"

    // PID numbers are only so long.
    //
    char *dir = strdup("/proc/PARENT_PID_AAAAAAAAAAAA/fd/");
    if(!dir)
        FAIL("strdup()");
    
    size_t len = strlen(dir);
    snprintf(dir, len, "/proc/%d/fd/", parentPid);

    printf("\n----- %s: list of open files from %s --------------\n",
            label, dir);

    execlp("ls", "-lt", "--color=auto", "--file-type", "--full-time",
            dir, NULL);

    FAIL("execlp()");
}

