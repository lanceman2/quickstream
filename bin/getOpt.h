
struct opts {
    const char *longOpt;
    int shortOpt; // char code like 'c'
};

// i is the current index
// 
// i should be initialized before first call to 1 
// for common use main(argc, argv)
//
// argv must be null terminated
//
// arg -- the user decides if arg is used.  If arg is used
//        then the user must increment i and set arg = 0.
//
// returns the next short option char, or 0 if at the end,
// or '*' and arg points to string if not an option
//
extern int getOpt(int argc, const char * const *argv, int *i,
        const struct opts *options/*array of options*/,
        const char **arg/*points to next possible arg*/);
