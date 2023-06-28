
// A simple block or a super block may have any number to configure
// callbacks that configure attributes.  We keep a dictionary of
// attributes in the simple and super blocks in the QsModule::attributes
// dictionary.  These so called attributes are created in the block's
// declare() function with qsAddConfig().
//
struct QsAttribute {

    // Unique name for this attribute, for the given block:
    char *name;

    // The block callback
    char *(*config)(int argc, const char * const *argv, void *userData);

    // description:
    char *desc;

    char *argSyntax;

    char *currentArgs;

    // If we keep a record of the last attribute config call by the
    // graph runner.
    //
    // If lastArgv == 0 then there is no record for this attribute.
    char **lastArgv;
    int lastArgc;

    // The last parent block (or graph) that configured this attribute.
    struct QsParentBlock *parentBlock;
};
