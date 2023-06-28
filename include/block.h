/************************************************************************
   block functions -- the part of libquickstream.so API used by any block,
                      simple or super
 ************************************************************************/

/** \defgroup block_functions block functions

@{
*/


#define QS_DEFAULT_MAXWRITE  ((size_t) 1024)
#define QS_DEFAULT_MAXREAD   QS_DEFAULT_MAXWRITE


// Option flag for qsGetMemory()
#define QS_GETMEMORY_RECURSIVE   001


// A failure return value for the return value from a qsAddConfig() block
// callback function.  Needed since 0 is a successful return value, and so
// it returning a malloc() allocated pointer.  We just needed a pointer
// value that is not valid.  The value -1 maps to a very large pointer
// address that is not valid.
//
#define QS_CONFIG_FAIL   ((char *) -1)


// We define these as bit masks for block types.
#define QS_TYPE_TOP       001
#define QS_TYPE_PARENT    002
#define QS_TYPE_JOBS      004
#define QS_TYPE_MODULE    010


enum QsBlockType {

    // not a module and can have children
    QsBlockType_parent = (QS_TYPE_PARENT),

    // The graph is a top block can have children.
    // It's parentBlock will be 0.
    QsBlockType_top = (QS_TYPE_PARENT | QS_TYPE_TOP),

    // not a module and cannot have children
    QsBlockType_jobs = (QS_TYPE_JOBS),

    // A module that can have children, but no jobs;
    // it can have jobs in it's children
    // built-in or DSO file loaded module
    QsBlockType_super = (QS_TYPE_MODULE | QS_TYPE_PARENT),

    // A module that cannot have children but has jobs
    // built-in or DSO file loaded module
    QsBlockType_simple = (QS_TYPE_MODULE | QS_TYPE_JOBS)
};


enum QsValueType {

    QsValueType_double = 1,
    QsValueType_uint64,
    QsValueType_bool,
    QsValueType_string32
};



struct QsGraph;
struct QsThreadPool;
struct QsBlock;
struct QsParameter;
struct QsInterBlockJob;
struct QsTask;


// DSO block options.
struct QsBlockOptions {

    enum QsBlockType type;
    bool disableDSOLoadCopy;
};




QS_EXPORT
const char *qsLibDir;

QS_EXPORT
const char *qsBlockDir;






QS_EXPORT
void qsSetNumInputs(uint32_t min, uint32_t max);

QS_EXPORT
void qsSetNumOutputs(uint32_t min, uint32_t max);


QS_EXPORT
void qsAdvanceInput(uint32_t inputPortNum, size_t len);



QS_EXPORT
void qsAdvanceOutput(uint32_t outputPortNum, size_t len);



QS_EXPORT void
qsAddRunFile(const char *fileName,
        bool runFileDSOCopyDisabled);



/*
In effect, arbitrarily complex input/output threshold conditions may be
set by the block flow() callback by not transferring any stream data until
it sees fit; but the flow() callback must transfer some data if all input
and output ports are at limits set by qsSetInputMax() and qsSetOutputMax()
(or the defaults), or the program will throw an exception.
*/

QS_EXPORT
void qsSetInputMax(uint32_t inputPortNum, size_t len);

QS_EXPORT
void qsSetOutputMax(uint32_t outputPortNum, size_t maxWriteLen);


QS_EXPORT
void qsMakePassThroughBuffer(uint32_t inPort, uint32_t outPort);

QS_EXPORT
void qsUnmakePassThroughBuffer(uint32_t inPort);



QS_EXPORT
void qsAddConfig(char *(*config)(int argc, const char * const *argv,
            void *userData),
        const char *name, const char *desc,
        const char *argSyntax, const char *defaultArgs);



QS_EXPORT
void qsSetUserData(void *userData);


QS_EXPORT
bool qsIsRunning(void);

QS_EXPORT
void qsDestroy(struct QsGraph *graph, int status);

QS_EXPORT
void qsStart(void);

QS_EXPORT
void qsStop(void);





QS_EXPORT
struct QsParameter *qsCreateSetter(const char *name, size_t size,
        enum QsValueType valueType, const void *initValue,
        int (*setCallback)(const struct QsParameter *p, const void *value,
            uint32_t readCount, uint32_t queueCount, void *userData));

QS_EXPORT
struct QsParameter *qsCreateGetter(const char *name, size_t size,
        enum QsValueType valueType, const void *initValue);

QS_EXPORT
void qsGetterPush(struct QsParameter *getter, const void *val);

QS_EXPORT
bool qsParameterIsConnected(struct QsParameter *p);



QS_EXPORT
struct QsSignalJob *qsSignalJobCreate(int sigNum,
        int (*callback)(void *userData), void *userData);


QS_EXPORT
void qsSignalJobDestroy(struct QsSignalJob *signalJob);



QS_EXPORT
void qsExist(struct QsGraph *graph, int status);



QS_EXPORT
struct QsInterBlockJob *qsAddInterBlockJob(void (*callback)(void *data),
        size_t size, uint32_t queueLength);

QS_EXPORT
void qsQueueInterBlockJob(struct QsInterBlockJob *job, void *data);


QS_EXPORT
void qsParseAdvance(uint32_t inc);

QS_EXPORT
int32_t qsParseInt32t(int32_t defaultVal);

QS_EXPORT
size_t qsParseSizet(size_t defaultVal);

QS_EXPORT
double qsParseDouble(double defaultVal);

QS_EXPORT
void
qsParseSizetArray(size_t defaultVal, size_t *array, size_t len);

QS_EXPORT
void
qsParseUint32tArray(uint32_t defaultVal, uint32_t *array, size_t len);

QS_EXPORT
void
qsParseUint64tArray(uint64_t defaultVal, uint64_t *array, size_t len);

QS_EXPORT
void
qsParseDoubleArray(double defaultVal, double *array, size_t len);

QS_EXPORT
void
qsParseBoolArray(bool defaultVal, bool *array, size_t len);


QS_EXPORT
void qsOutputDone(uint32_t portNum);


QS_EXPORT
const char *qsBlockGetName(void);


// Very simple wrapper.
QS_EXPORT
void *qsGetDLSymbol(void *dlhandle, const char *symbol);


// Very simple dlclose(3) wrapper that pairs with
// qsOpenRelativeDLHandle().
QS_EXPORT
void qsCloseDLHandle(void *dlhandle);

// This adds finding the DSO file in the same directory as the
// as the DSO block that is calling this; but otherwise not much
// more than a wrapper of dlopen(3).
QS_EXPORT
void *qsOpenRelativeDLHandle(const char *relpath);


/* Get memory in libquickstream.so that was
made the first time this is called by the current process.  Ya, it's a
process wide pointer that is shared between threads via a key (name).

More simply it's raw inter-thread shared memory.

It may seem strange, but is required/useful to store data for blocks when
the block don't have a way to share and store the data by other common
means like declaring a global static variable in a particular block, when
you don't know what block will have it until run-time.  Using the block
that calls this first will dynamically allocate a pointer via malloc(3)
and/or family of functions; and then the pointer will be accessible to any
function that calls qsGetMemory() the proceeding times after the first
call.

Thread safety: Safe when used correctly.  The user can fuck-up memory
by using the returned value incorrectly.  Not for general block use.
You must me an expert multi-threaded programmer to use this.  It is
assumed that this is only freed by libquickstream.so in it's library
destructor; that is unless we add a "free function", which we may.

This is like the "rawest" inter-thread communication method that there can
be.  It can't exist without a inter-thread common library, given the
threads that use it do not share any compiled code except the common
library (in this case liqquickstream.so).  I think most people will not
appreciate this, but then again most people can't code for shit; they are
not open minded enough.  If you do not know what a mutex is do not
fucking use this.  Pretty much, for the most part, don't fucking use
this.

RANT:
It's like a GNUradio Variable.  The problem with GNUradio Variables is
that they are not robust for use by anyone other than expert
multi-threaded programmers using the libgnuradio API.  You can make
programs with just the high level GUI program gnuradio-companion that
corrupt inter-block-thread memory.  It's by design.  To me that is
unacceptable.  Yes, I told many people; but alas, I'm insane by current
standard; this is just rantings of a madman.

For the first thread to call this with this key (name) the qsGetMemory()
returns while holding the mutex lock pointed to by *mutex.  So the calling
thre will need to call pthread_mutex_unlock(*mutex) sometime after calling
qsGetMemory(), but only if *firstCaller was set.  Without this stupid
*mutex this will have some nasty inter-thread race conditions inherit to
its' use.

Yes, yes; we know that all memory in the program address space is
inter-thread shared memory, but we need this memory to come from code that
is not originating from in a block so that it can be shared by all blocks
that exist now and may not exist in the future, and all new blocks too.
Memory for all the blocks that is not from any block.
*/
QS_EXPORT
void *qsGetMemory(const char *name, size_t size,
        pthread_mutex_t **mutex, bool *firstCaller, uint32_t flags);

QS_EXPORT
void qsFreeMemory(const char *name);


/** \def QS_USER_DATA_TYPE

The \c QS_USER_DATA_TYPE C-preprocessor macro may be defined in the blocks
code to tell quickstream.h what is the type of pointer that is block's
user data is that is passed to the block's common callback functions like:
for an example see undeclare().
*/
#ifndef QS_USER_DATA_TYPE
#  define QS_USER_DATA_TYPE  void *
#endif



/** \page run modes

Running computer programs can always encounter errors.  Care must be
taken to clarify what the "error" is at many levels.  An error in one
API (application programming interface) may not be an error that is seen
by the top level user, or just one layer above that API in a program
layer.  Because of errors at different levels of a running program
defining running mode abstractions is unavoidable.  A program can try
to read a USB device, but if the device is not plugged in a decision
must be made by the program that is trying and failing to read the
non-existent hardware.  It can be advantageous for the program to continue
running after failing to read the device so the user of the program can
save state before quitting or plug in the device and than recover and
continue using the running program.  Since quickstream is a API framework
it requires running modes to be defined so that decisions brought on by
errors can be made by block module developers, quickstream runner
developers, and end application users.  A very common error mode would be
when a file is not found, be it a block module DSO (dynamic shared object)
or an input data file.  Exiting the program may not be the best choose
in all cases.  We must let there be new run mode abstractions defined in
order to provide chooses for the developer and users in the different
layers/levels.

For block developers the return codes from block callback functions will
put the block and/or the graph into different states.

Hence the return values from block callback functions are very important
for controlling the state of the running quickstream program.

All commonly named block callback functions are optional.

You can make a DSO block with an empty file.  The GNU/Linux linker/loader
system can load a file compiled from an empty file.  Such a block does not
do a lot, but it is a valid quickstream block that can be loaded and built
into any quickstream graph program.  It's a very handy test case.  This
zero size source code provides a very good bragging right for quickstream.
It's proof that it has a very good/pure design that is system friendly.
If you can't follow that you do not understand basics of operating
systems, and you're are likely a bloat monger, like most programmers
today.

*/


/** declare a block

A function that defines/declares a block in the block.

\todo example block source code here.

declare() will be called the running thread that called
qsGraph_createBlock() and if the block is a super block the thread may
recurse and call qsGraph_createBlock() from declare() any number of time
(assuming we do not blow the function call stack).  And so, declare() is
never called by a thread pool worker thread.

declare() defines control parameters and the number of input and output
streams that can be be connected to other blocks.
declare() defines configurable attributes that may be set by runner
programs and/or super blocks that load the block.

If the block is a super block declare() may load other blocks within it.
If the block is a super block declare() may make connections and
configure attributes within blocks it loads.

The return value returned by the block code will determine if the block
is kept as loaded, or unloaded due to a failure the writer of the block
decided that it could be recover from.

\return
   1. 0 for success and the block stays loaded; or
   2. greater than 0 for failure and undeclare() is called is it exists,
      and the block is unloaded; and
   3. less than 0 for failure and not to call undeclare() and the block is
      unloaded.
*/
extern
int declare(void);


/** Block's undeclare() callback

The block writer may define this function to clean up system resources
which may have been created.  This undeclare() function will be called
just before the block module is unloaded.

The block writer may define the CPP (C pre-processor) macro
\c QS_USER_DATA_TYPE as something like:

\code
#define QS_USER_DATA_TYPE  struct MyStruct *
#include <quickstream.h>
\endcode

before including the quickstream.h header file so as to make the function
prototype match what the block writer uses.  Like for example:

\code
int undeclare(struct MyStruct *x);
\endcode

\return 0 for success, and non-zero on failure.  There is currently no state
changes due to returning a non-zero.

\todo make this function void in place of int??
*/
extern
int undeclare(QS_USER_DATA_TYPE userData);


/** A block's stream flow (work) function

If a block can have stream inputs and/or outputs it is required to have a
function named flow().

\return 0 to continue running the stream, 1 to finish the stream run by
propagating a flush through the stream and flush all of its outputs.

\sa qsOutputFlush().

As a general principle simple blocks in the stream do not directly control
the stream, but blocks that are not in the stream can control the stream;
except that blocks in the stream can report that they are done giving
output to the stream (flushing).  If block in the stream could control the
stream then we would lose programmability and function at the block/graph
builder level, making the quickstream software package effectively
useless.  Super blocks are not in the stream.

When stream data enters a flushed output, the data is not sent to the
output and the data is ignored.  A block writes data directly to a flushed
output, that it owns, the write is ignored (has no effect).

In the case of a error internal to the block the block cannot stop the
stream run, so the block must write the flow() function so that it can be
continued to be called in the case of things like a system read or write
failure.  The block needs to recover for the next stream run or fail at
all future stream starts if recovery is not possible.  The
libquickstream.so API should be assumed to be robust by the block writer;
if not please report a bug.
*/

extern
int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        QS_USER_DATA_TYPE userData);

extern
int flush(const void * const in[], const size_t inLens[],
        void * const out[], const size_t outLens[],
        uint32_t numInputs, uint32_t numOutputs,
        QS_USER_DATA_TYPE userData);

extern
int construct(QS_USER_DATA_TYPE userData);

/** A block's stream optional start() callback

The block's start() is called for the block, if it exists, every time the
graphs stream starts, from qsGraph_start(), unless it is removed by the
block.

If a block incurs a failure, like due to a common system error file not
found or like thing, the block needs to let start() be called again, like
when the user plugs in a hardware device while the program is running; so
that the quickstream program can keep running.

Blocks that do not handle common system errors will never be let into
the core quickstream blocks.  Bad blocks are blocks that exit on start()
failures.

The limited failure modes from the start() (and same for construct())
return values reflect the state just for one stream start and stop cycle.
Future start and stop sequences are considered mostly independent.  If a
block gets in a state such that it can't ever run the stream, the block
needs to keep a flag so that it may keep returning a non-zero return
value, and stop calling the "driver code".  The block user can still
develop other graphs and blocks with the running code after the block
fails internally.  Block failures must be tolerated by the blocks code.
The block can only make request to the run library for not running the
stream or not calling other callbacks for the block; all depending on the
operating system configuration and libquickstream.so compile options,
and libquickstream.so environment variable values.

Clearly there are some failures that no running program can deal with like
failing to allocate memory.  libquickstream.so tends to assert in that
case causing either a hung debugging session, a program exit, and maybe a
core dump.

\return
   1. 0 on success and continue calling all the graph's block's start()
      functions;
   2. -1 if block requests starting of the stream not complete
      and the rest of the block start() calls not happen for this
      particular stream start sequence and this block's stop not
      be called for this stream run; and
   3. 1 if block requests starting of the stream not complete
      and the rest of the block start() calls not happen for this
      particular stream start sequence and this block's stop
      be called, if it exists, for this stream run;

*/
extern
int start(uint32_t numInputs, uint32_t numOutputs,
        QS_USER_DATA_TYPE userData);
extern
int stop(uint32_t numInputs, uint32_t numOutputs,
        QS_USER_DATA_TYPE userData);

extern
int destroy(QS_USER_DATA_TYPE userData);


#undef QS_USER_DATA_TYPE


/** @} */ // Close doxygen defgroup block_functions
