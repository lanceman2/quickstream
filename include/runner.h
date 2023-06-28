/************************************************************************
   runner functions -- the part of libquickstream.so API used to run,
                       stop, create super blocks from any flow graph,
                       and commandeer a running quickstream program 
 ***********************************************************************/

/** \defgroup runner_functions runner functions



@{
*/

/** qsGraph_create() bit mask flags */
#define QS_GRAPH_IS_MASTER          00001
#define QS_GRAPH_SAVE_ATTRIBUTES    00002
#define QS_GRAPH_HALTED             00004 // create with graph wide halt



QS_EXPORT
struct QsGraph *qsGraph_create(const char *path,
        uint32_t maxThreads,
        const char *name, const char *threadPoolName,
        uint32_t flags);

QS_EXPORT
void qsGraph_destroy(struct QsGraph *graph);


QS_EXPORT
struct QsThreadPool *qsGraph_createThreadPool(
        struct QsGraph *graph, uint32_t maxThreads,
        const char *name);

QS_EXPORT
struct QsThreadPool *qsGraph_getThreadPool(struct QsGraph *graph,
        const char *name);

QS_EXPORT
uint32_t qsGraph_getNumThreadPools(struct QsGraph *graph);


QS_EXPORT
void qsGraph_setDefaultThreadPool(struct QsGraph *graph,
        struct QsThreadPool *threadPool);


QS_EXPORT
void qsThreadPool_setMaxThreads(struct QsThreadPool *threadPool,
        uint32_t maxThreads);

QS_EXPORT
void qsThreadPool_destroy(struct QsThreadPool *threadPool);


QS_EXPORT
int qsGraph_wait(struct QsGraph *graph, double seconds);

QS_EXPORT
int qsGraph_waitForStream(struct QsGraph *g, double seconds);

QS_EXPORT
int qsGraph_waitForDestroy(struct QsGraph *g, double seconds);

QS_EXPORT
int qsParameter_setValueByString(struct QsParameter *p,
        int argc, const char * const *argv);

QS_EXPORT
int qsGraph_printDot(const struct QsGraph *graph, FILE *file,
        uint32_t optionsMask);

QS_EXPORT
int qsGraph_printDotDisplay(const struct QsGraph *graph,
        bool waitForDisplay, uint32_t optionsMask);


QS_EXPORT
struct QsGraph *qs_getGraph(const char *gname);


QS_EXPORT
void qsGraph_halt(struct QsGraph *graph);

QS_EXPORT
void qsGraph_unhalt(struct QsGraph *graph);


QS_EXPORT
void qsThreadPool_addBlock(struct QsThreadPool *threadPool,
        struct QsBlock *block);

static inline
void qsBlock_setThreadPool(struct QsBlock *block,
        struct QsThreadPool *threadPool) {
    qsThreadPool_addBlock(threadPool, block);
}


QS_EXPORT
int qsGraph_start(struct QsGraph *graph);


QS_EXPORT
int qsGraph_stop(struct QsGraph *graph);


QS_EXPORT
void qsGraph_lock(struct QsGraph *graph);

QS_EXPORT
void qsGraph_unlock(struct QsGraph *graph);


QS_EXPORT
void qsGraph_launchRunner(struct QsGraph *graph);



QS_EXPORT
int qsGraph_setMetaData(struct QsGraph *g,
        const char *key, const void *ptr, size_t size);

QS_EXPORT
void *qsGraph_getMetaData(const struct QsGraph *g,
        const char *key, size_t *ret_size);

QS_EXPORT
void qsGraph_clearMetaData(struct QsGraph *g);


QS_EXPORT
void qsGraph_saveConfig(struct QsGraph *graph, bool doSave);

QS_EXPORT
void qsGraph_removeConfigAttribute(struct QsBlock *block,
        const char *aName);

QS_EXPORT
int qsGraph_saveSuperBlock(const struct QsGraph *graph,
        const char *path, uint32_t optsFlag);


QS_EXPORT
int qsGraph_save(const struct QsGraph *graph,
        const char *path, const char *gpath,
        uint32_t optsFlag);

QS_EXPORT
const char *qsGraph_getName(const struct QsGraph *g);

QS_EXPORT
const char *qsThreadPool_getName(const struct QsThreadPool *tp);

QS_EXPORT
int qsThreadPool_setName(struct QsThreadPool *tp, const char *name);


QS_EXPORT
int qsGraph_setName(struct QsGraph *g, const char *name);


QS_EXPORT
const char *qsBlock_getName(const struct QsBlock *b);


QS_EXPORT
int qsBlock_rename(struct QsBlock *b, const char *name);






/** @} */ // Close doxygen defgroup runner_functions
