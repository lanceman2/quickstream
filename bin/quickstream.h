
extern
int level;

// Current/last graph:
extern
struct QsGraph *graph;
extern
uint32_t numGraphs;
extern
struct QsGraph **graphs;


// help() does not return, it will exit.
extern
void help(int fd);

extern
int ProcessCommand(int comm, int numArgs, const char *command,
        const char * const *args);

extern
void CreateGraph(void);


extern
int RunInterpreter(const char *filename);
