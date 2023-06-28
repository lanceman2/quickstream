#define MAX_JOBS  10


struct Base {

    void (*AddJob)(struct QsInterBlockJob *j);

    uint32_t numJobs;
    struct QsInterBlockJob *jobs[MAX_JOBS];

    pthread_mutex_t mutex;
};

