
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
        const char * const *args, int *exitStatus);

extern
void CreateGraph(void);


extern
int RunInterpreter(int numArgs, const char * const *argv);


extern
void Cleanup(void);
