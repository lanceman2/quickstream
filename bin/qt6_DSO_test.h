
#ifdef __cplusplus
extern "C" {
#endif
    void RunApp(int argc, char* argv[]);
#ifdef __cplusplus
}
#endif


static inline
void Wait(const char *note = 0) {
    if(note)
        printf("\n%s\n\n", note);
    printf("Run: ls -thlF --color=auto /proc/%d/fd\n", getpid());
    printf("<enter> to continue\n");
    getchar();
    printf("\n");
}

