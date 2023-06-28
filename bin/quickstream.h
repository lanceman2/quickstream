
// Needed for ../lib/quickstream/misc/qsOptions.h
//
struct opts {
    const char *longOpt;
    int shortOpt; // char code like 'c'
};


extern
int exitStatus;

extern
bool exitOnError;

extern
int spewLevel;


// i is the current index
// 
// i should be 1 first call to this
// for common use main(argc, argv)
//
// argv must be null terminated
//
// returns the next short option char, or 0 if at the end,
// or '*'
//
// command gets set to point to a static string that is the
// long option like for example: "--help" or "*" if there
// was none found.
//
extern int getOpt(int argc, const char * const *argv, int i,
        const struct opts *options/*array of options*/,
        const char **command);

extern
bool RunCommand(int c, int argc, const char *command,
        const char * const * argv);

extern
void help(int fd, const char *opt);


extern
int RunInterpreter(int numArgs, const char * const *arg);
