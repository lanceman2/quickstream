/************************************************************************
   builder functions -- the part of libquickstream.so API used to load,
                        connect, and configure blocks
 ***********************************************************************/

/** \defgroup builder_functions builder functions


@{
*/

enum QsPortType {

    QsPortType_none = 0,
    QsPortType_setter = 1, // setter control parameter
    QsPortType_getter,     // getter control parameter
    QsPortType_input,      // stream input
    QsPortType_output      // stream output
};


struct QsPort;


QS_EXPORT
void qsParameter_setValue(struct QsParameter *p, const void *val);


QS_EXPORT
struct QsParameter
*qsBlock_getGetter(const struct QsBlock *block, const char *name);


QS_EXPORT
struct QsParameter
*qsBlock_getSetter(const struct QsBlock *block, const char *name);



QS_EXPORT
struct QsBlock *qsGraph_createBlock(struct QsGraph *graph,
        struct QsThreadPool *tp,
        const char *path, const char *name,
        void *userData);



QS_EXPORT
struct QsBlock *qsGraph_getBlock(struct QsGraph *graph,
        const char *bname);

QS_EXPORT
struct QsPort *qsBlock_getPort(struct QsBlock *block,
        enum QsPortType pt, const char *pname);

QS_EXPORT
int qsGraph_connect(struct QsPort *pA, struct QsPort *pB);

QS_EXPORT
int qsGraph_connectByStrings(struct QsGraph *graph,
        const char *blockA, const char *typeA, const char *nameA,
        const char *blockB, const char *typeB, const char *nameB);

QS_EXPORT
int qsGraph_connectByBlock(struct QsGraph *graph,
        struct QsBlock *blockA, const char *typeA, const char *nameA,
        struct QsBlock *blockB, const char *typeB, const char *nameB);


QS_EXPORT
int qsBlock_config(struct QsBlock *block,
            int argc, const char * const *argv);

QS_EXPORT
int qsBlock_configV(struct QsBlock *block, ...);

QS_EXPORT
int qsBlock_configVByName(struct QsGraph *g,
        const char *bname, ...);

QS_EXPORT
int qsBlock_destroy(struct QsBlock *block);

QS_EXPORT
bool qsParameter_isGetterGroup(struct QsParameter *p);


QS_EXPORT
size_t qsParameter_getValue(const struct QsParameter *p,
        void *val, size_t size);


/** get the type of a  parameter

\param p

\return the type of the parameter.
*/
QS_EXPORT
enum QsValueType qsParameter_getValueType(struct QsParameter *p);


/** get the size of a  parameter

\param p

\return the size of the parameter in bytes.
*/
QS_EXPORT
size_t qsParameter_getSize(struct QsParameter *p);


QS_EXPORT
int qsBlock_makePortAlias(
        struct QsGraph *g,
        const char *toBlockName,
        const char *portType,
        const char *toPortName,
        const char *aliasPortName);

QS_EXPORT
void qsGraph_removePortAlias(struct QsGraph *graph,
        enum QsPortType type,
        const char *name);


QS_EXPORT
int qsGraph_disconnectByStrings(struct QsGraph *g,
        const char *blockName, const char *portType,
        const char *portName);

QS_EXPORT
int qsGraph_disconnect(struct QsPort *port);


QS_EXPORT
int qsParameter_disconnect(struct QsParameter *parameter);



/** @} */ // Close doxygen defgroup builder_functions

