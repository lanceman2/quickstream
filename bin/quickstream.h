
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


// Count the number of arguments up to one that starts with '-'
//
// i  is the current argv index.
//
// return the number of arguments after the first argument.
static inline
int GetNumArgs(int i, int argc, const char * const *argv) {

    int numArgs = 0;
    for(++i; i<argc && argv[i][0] != '-'; ++i)
        ++numArgs;
    return numArgs;
}


