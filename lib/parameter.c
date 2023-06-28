#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../include/quickstream.h"

#include "debug.h"
#include "Dictionary.h"

#include "c-rbtree.h"
#include "name.h"
#include "threadPool.h"
#include "block.h"
#include "graph.h"
#include "job.h"
#include "port.h"
#include "parameter.h"

#include "parseBool.h"


// We use this mutex for all setters before they are connected.
// We do not need more than one mutex for this case.
//
// There is only a "master" thread (non-worker thread) that may set the
// setter value when the setter is not connected to a parameter connection
// group.
//
static
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// We stop rapid writes to setters, and effectively serialize the writing
// to setters from the non-worker thread (master-like thread).
//
// I can across a "bug" when setting one setter in two successive commands
// with the command line quickstream program.  Most of the time only
// one value was set and the other command was effectively skipped.
// The conditional variable and two flags, busy and waiting effectively
// serialize the setting of the setter and the reading of it by the
// block (in the thread pool worker thread.
static
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static
bool busy = false, waiting = false;




static inline void
CheckForOverRun(struct QsParameter *p, struct QsSetter *s,
        struct QsGroup *g) {

    DASSERT(g->value);

    if(g->writeCount - s->readCount > g->numValues) {

        // This setter read the values too slowly and the buffer
        // over-ran the setter read pointer, p->value.
        //
        WARN("block:parameter %s:%s queue over-run (queue length = %"
                PRIu32 ") lost %" PRIu32 " values",
                p->port.block->name, p->port.name,
                parameterQueueLength,
                g->writeCount - s->readCount - g->numValues);

        s->readCount = g->writeCount - g->numValues;
        // go to the current group value that is next;
        // which is the oldest value that was written in the ring buffer.
        //
        // When we read all the values in the ring buffer we will be back
        // to this value pointer and the Counters will match.
        p->value = g->value;
    }
}


static inline void
SetSetterToLastGroupValue(struct QsSetter *s, struct QsGroup *g) {

    DASSERT(g);
    DASSERT(s);
    // There must be at least one value set in the group.
    DASSERT(g->value);
    DASSERT(g->values);
    struct QsParameter *p = (void *) s;
    DASSERT(p->port.portType == QsPortType_setter);
    // We must be resetting the setter value, otherwise we should not
    // be initializing the setter value like this.  This is not the case
    // where the setter is keeping track of values in the queue.  It does
    // not know how many values are present at this point.  But we know
    // there is at least one value, because g->value is set.
    //

    // The group write pointer should not under-run or overrun the
    // array (ring buffer):
    DASSERT(g->value >= g->values);
    DASSERT(g->value < g->values + g->numValues * g->size);

    p->value = g->value - g->size;
    s->readCount = g->writeCount - 1;

    if(p->value < g->values)
        // We indexed too far back, so wrap in the ring buffer forward the
        // total length of the array.  p->value will point to the last
        // element:
        p->value += g->numValues * g->size;
}


// The setters have a job that calls the setter callback() with all the
// values that are queued in the group ring buffer.
//
// Hence this is called by a thread pool worker thread.
//
static bool Work(struct QsJob *j) {

    struct QsSetter *s = ((void *)j) - offsetof(struct QsSetter, job);
    DASSERT(s->callback);
    struct QsParameter *p = (void *) s;
    struct QsGroup *g = p->group;
    DASSERT(g, "Parameter group not set");

    DASSERT(g->writeCount != s->readCount);

    CheckForOverRun(p, s, g);

    if(!p->value)
        // This is the first time this setter has a value to read.
        SetSetterToLastGroupValue((void *) p, g);

    // Copy the "parameter" value into stack memory.
    uint8_t val[p->size];
    memcpy(val, p->value, p->size);

    // Setup the ring buffer read pointer and counter for next time.
    ++s->readCount;
    p->value += p->size;
    if(p->value >= g->values + g->numValues*p->size)
        // Loop the ring buffer read pointer back.
        p->value -= g->numValues*p->size;


    qsJob_unlock(j); // Unlock the parameter group mutex.

    struct QsWhichBlock stackSave;
    SetBlockCallback((void *) j->jobsBlock,
                CB_SET, &stackSave);

    // Call the callback() passing a pointer to stack memory, so no mutex
    // lock is needed when calling.  That's the whole point of this
    // "control parameter" interface.
    //
    // We pass readCount and writeCount so that the user can detect an
    // overrun.
    //
    s->callback(p, val, s->readCount, g->writeCount,
            p->port.block->userData);

    RestoreBlockCallback(&stackSave);

    qsJob_lock(j);


    if(s->readCount != g->writeCount)
        // keep calling.
        return true;

    return false;
}


static inline void
DestroyGroup(struct QsGroup *g, size_t size) {

    DASSERT(g);
    DASSERT(g->values);

    CHECK(pthread_mutex_destroy(&g->mutex));

    DZMEM(g->values, g->numValues*size);
    free(g->values);

    DZMEM(g, sizeof(*g));
    free(g);
}


// The work function for a setter that is not connected.
// We don't use this when a parameter is connected.
//
static
bool setterWork(struct QsJob *j) {

    DASSERT(j);
    struct QsSetter *s = ((void *)j) - offsetof(struct QsSetter, job);
    DASSERT(s->callback);
    struct QsParameter *p = (void *) s;

    uint32_t readCount = s->readCount;

    DASSERT(p->value);
    // This setter parameter cannot be connected to other parameters:
    DASSERT(!p->group);

    // Copy the "parameter" value into stack memory.
    uint8_t val[p->size];
    memcpy(val, p->value, p->size);

    busy = true;

    qsJob_unlock(j);

    struct QsWhichBlock stackSave;
    SetBlockCallback((void *) j->jobsBlock,
                CB_SET, &stackSave);

    s->callback(p, val, 0/*read count*/, 0/*queue count*/,
            p->port.block->userData);

    RestoreBlockCallback(&stackSave);

    qsJob_lock(j);

    busy = false;

    if(waiting)
        CHECK(pthread_cond_signal(&cond));

    // Check if we got another value while calling s->callback().
    if(s->readCount != readCount)
        // We got another value while calling s->callback above.
        return true; // true == call this again.

    return false;
}


static inline void
DisconnectParameter(struct QsParameter *p, struct QsGroup *g) {

    DASSERT(p);
    DASSERT(g);
    DASSERT(p->group == g);
    DASSERT(g->numParameters);

    if(p->port.portType == QsPortType_setter) {
        DASSERT(g->sharedPeers);
        DASSERT(*g->sharedPeers);
        struct QsJob *j = ((void *) p) + offsetof(struct QsSetter, job);
        qsJob_cleanup(j);
        qsJob_init(j, (void *) p->port.block, setterWork, 0, 0);
        qsJob_addMutex(j, &mutex);

    } else {
        DASSERT(p->port.portType == QsPortType_getter);
        DASSERT(g->getter == (void *) p);
        p->group->getter = 0;
    }
    // A disconnected parameter is a parameter that is not in a group.
    p->group = 0;
    --g->numParameters;


    if(g->value) {
        // Get the last value from the group ring buffer.
        void *lastValue = g->value - p->size;
        if(lastValue < g->values)
            lastValue = g->values + (g->numValues - 1)*p->size;
        p->value = malloc(p->size);
        ASSERT(p->value, "malloc(%zu) failed", p->size);
        memcpy(p->value, lastValue, p->size);
    } else
        // We have no values yet.
        DASSERT(p->value == 0);

    // Unmark the parent block that made the connection.
    p->port.parentBlock = 0;


    // See if we need the group now.  A group with one parameter is not a
    // usable group.
    if(g->numParameters == 1) {
        if(g->getter) {
            DASSERT(!g->sharedPeers);
            DisconnectParameter((void *) g->getter, g);
        } else {
            DASSERT(g->sharedPeers);
            DASSERT(*g->sharedPeers);
            // Disconnect the first parameter in the list of
            // parameters/jobs.
            DisconnectParameter(((void *) (*g->sharedPeers)) -
                    offsetof(struct QsSetter, job), g);
        }
        return;
    }

    if(g->numParameters == 0)
        DestroyGroup(g, p->size);
}


static void
DestroyParameter(struct QsParameter *p) {

#if 0
    DSPEW("Removing %cetter parameter \"%s:%s\"",
            (p->port.portType == QsPortType_setter)?'s':'g',
            p->port.block->name,
            p->port.name);
#endif
    if(p->group)
        DisconnectParameter(p, p->group);

    DASSERT(!p->group);

    if(p->value) {
        DZMEM(p->value, p->size);
        free(p->value);
        p->value = 0;
    }


    if(p->port.portType == QsPortType_setter)
        qsJob_cleanup(((void *) p) + offsetof(struct QsSetter, job));


    DZMEM(p->port.name, strlen(p->port.name));
    free(p->port.name);

#ifdef DEBUG
    size_t psize;

    if(p->port.portType == QsPortType_setter)
        psize = sizeof(struct QsSetter);
    else
        psize = sizeof(struct QsGetter);
    DZMEM(p, psize);
#endif
    free(p);
}


static inline
void *CreateParameter(size_t psize/* struct size */,
        enum QsValueType vtype, size_t vsize/* value size*/,
        const char *name, const void *initValue,
        struct QsDictionary *dict, struct QsSimpleBlock *b) {

    DASSERT(vtype);
    DASSERT(psize);
    ASSERT(name && name[0]);
    ASSERT(vsize);

    struct QsParameter *p = calloc(1, psize);
    ASSERT(p, "calloc(1,%zu) failed", psize);
    p->valueType = vtype;
    p->size = vsize;

    p->port.name = GetUniqueName(dict, 0, name, 0);
    ASSERT(p->port.name);
    p->port.block = (void *) b;

    struct QsDictionary *d;
    ASSERT(qsDictionaryInsert(dict, p->port.name, p, &d) == 0);
    qsDictionarySetFreeValueOnDestroy(d,
            (void (*)(void *)) DestroyParameter);

    switch(vtype) {
        case QsValueType_double:
            ASSERT(0 == vsize % sizeof(double));
            break;
        case QsValueType_uint64:
            ASSERT(0 == vsize % sizeof(uint64_t));
            break;
        case QsValueType_bool:
            ASSERT(0 == vsize % sizeof(bool));
            break;
        case QsValueType_string32:
            ASSERT(0 == vsize % 32);
            ASSERT(vsize == 32, "Need to add support for "
                    "N arrays of control "
                    "parameters as a 32 byte string");
            break;
         default:
            ASSERT(0, "Bad parameter value type %d", vtype);
            break;
    }

    if(initValue) {
        p->value = malloc(vsize);
        ASSERT(p->value, "malloc(%zu) failed", vsize);
        memcpy(p->value, initValue, vsize);
    }

    return p;
}


struct QsParameter *qsBlock_getGetter(const struct QsBlock *b,
        const char *name) {

    // Note: this function is almost the same as qsBlock_getSetter()
    //
    DASSERT(b);
    ASSERT(b->type == QsBlockType_simple);
    struct QsJobsBlock *jb = (void *) b;

    CHECK(pthread_mutex_lock(&jb->block.graph->mutex));

    struct QsModule *m = ((void *) b) +
        offsetof(struct QsSimpleBlock, module);

    DASSERT(m->ports.getters);

    struct QsParameter *p = qsDictionaryFind(m->ports.getters, name);

    CHECK(pthread_mutex_unlock(&jb->block.graph->mutex));

    return p;
}


struct QsParameter *qsBlock_getSetter(const struct QsBlock *b,
        const char *name) {

    // Note: this function is almost the same as qsBlock_getGetter()
    //
    DASSERT(b);
    ASSERT(b->type == QsBlockType_simple);
    struct QsJobsBlock *jb = (void *) b;

    CHECK(pthread_mutex_lock(&jb->block.graph->mutex));

    struct QsModule *m = ((void *) b) +
        offsetof(struct QsSimpleBlock, module);

    DASSERT(m->ports.getters);

    struct QsParameter *p = qsDictionaryFind(m->ports.setters, name);

    CHECK(pthread_mutex_unlock(&jb->block.graph->mutex));

    return p;
}

struct QsParameter *qsCreateSetter(const char *name, size_t size,
        enum QsValueType vtype, const void *initValue,
        int (*callback)(const struct QsParameter *p, const void *value,
            uint32_t readCount, uint32_t writeCount, void *userData)) {

    NotWorkerThread();

    DASSERT(callback);

    struct QsSimpleBlock *b = GetBlock(CB_DECLARE|CB_CONFIG, 0,
            QsBlockType_simple);
    struct QsModule *m = &b->module;

    DASSERT(b->jobsBlock.block.graph);

    // We are in declare() or config callbacks so we already have the
    // graph mutex lock.


    struct QsSetter *s = CreateParameter(sizeof(*s), vtype, size,
            name, initValue, m->ports.setters, b);

    s->parameter.port.portType = QsPortType_setter;
    s->callback = callback;

    qsJob_init(&s->job, (void *) b, setterWork, 0, 0);
    qsJob_addMutex(&s->job, &mutex);


    return (struct QsParameter *) s;
}


struct QsParameter *qsCreateGetter(const char *name, size_t size,
        enum QsValueType vtype, const void *initValue) {

    NotWorkerThread();

    struct QsSimpleBlock *b = GetBlock(CB_DECLARE|CB_CONFIG, 0,
            QsBlockType_simple);
    struct QsModule *m = &b->module;

    DASSERT(b->jobsBlock.block.graph);

    // We are in declare() or config callbacks so we already have the
    // graph mutex lock.

    struct QsGetter *g = CreateParameter(sizeof(*g), vtype,
            size, name, initValue, m->ports.getters, b);

    g->parameter.port.portType = QsPortType_getter;


    return (struct QsParameter *) g;
}


static inline void
PushGroupValue(struct QsGroup *g, const void *val, size_t size) {

    CHECK(pthread_mutex_lock(&g->mutex));

    DASSERT(g->values);
    DASSERT(g->sharedPeers);
    DASSERT(*g->sharedPeers);
    DASSERT(g->numValues);
    DASSERT(g->value < g->values + g->numValues*size);

    if(!g->value) {
        DASSERT(g->writeCount == 0);
        g->value = g->values;
    }

    DASSERT(g->value >= g->values);

    // * Write the value.
    memcpy(g->value, val, size);
    ++g->writeCount;

    // * go to the next values[]
    g->value += size;
    if(g->value >= g->values + g->numValues*size)
        // We are at the end; go to the first element.
        g->value = g->values;

    for(struct QsJob **j = g->sharedPeers; *j; ++j)
        qsJob_queueJob(*j);

    CHECK(pthread_mutex_unlock(&g->mutex));
}


void qsGetterPush(struct QsParameter *getter, const void *val) {

#ifdef DEBUG
    struct QsModule *m = 0;
    //
    // This can be called by a 1. thread pool worker thread or 2. a block
    // callback like declare() or configure which are not from a thread
    // pool worker thread.  And not any other case.
 
    {
       struct QsWhichJob *wj = pthread_getspecific(threadPoolKey);
        if(wj) {
            // 1. This is a worker thread.
            ASSERT(wj->job);
            ASSERT(wj->job->jobsBlock);
            ASSERT(wj->job->jobsBlock->block.type == QsBlockType_simple);
            m = ((void *) wj->job->jobsBlock) +
                offsetof(struct QsSimpleBlock, module);
        }
    }
    if(!m) {
        // 2. This is called from a block callback like declare() or
        // configure.
        NotWorkerThread();
        m = GetModule(CB_DECLARE|CB_CONFIG, QsBlockType_simple);
    }

    ASSERT(getter->port.block == ((void *) m) -
            offsetof(struct QsSimpleBlock, module));
#endif

    DASSERT(getter);
    DASSERT(getter->port.portType == QsPortType_getter);
    DASSERT(getter->size);

    struct QsGroup *g = getter->group;

    if(g) {
        // This getter is connected.
        DASSERT(g->getter == (void *) getter);
        PushGroupValue(g, val, getter->size);
        return;
    }

    // This getter is not connected.
    //
    // Note: we are not saving/queuing older values.

    if(!getter->value) {
        getter->value = malloc(getter->size);
        ASSERT(getter->value, "malloc(%zu) failed", getter->size);
    }

    // In this case we are just storing the value for when and if this
    // getter gets connected to some setter parameters.
    memcpy(getter->value, val, getter->size);
}


int qsParameter_setValueByString(struct QsParameter *p,
        int argc, const char * const *argv) {

    NotWorkerThread();

    DASSERT(p);
    ASSERT(p->port.portType == QsPortType_setter);
    DASSERT(p->port.block->name);
    DASSERT(p->port.block->name[0]);
    DASSERT(p->size);

    if(p->group && p->group->getter) {
        ERROR("Parameter \"%s:%s\" is in a getter group",
                p->port.block->name, p->port.name);
        return 1;
    }

    int i = 0;
    uint32_t arrayLength;

    switch(p->valueType) {

        case QsValueType_bool:
            {
                DASSERT(p->size % sizeof(bool) == 0);
                arrayLength = p->size/sizeof(bool);
                DASSERT(arrayLength);
                // We'll use stack memory for stuffing the values to and
                // then copy it to the parameter values.
                bool value[arrayLength];
                // Set default first value:
                bool lastVal = false;
                bool *val = value;
                // default value:
                for(; i<argc && i<arrayLength; ++i) {
                    *val = qsParseBool(argv[i]);
                    lastVal = *val;
                    ++val;
                }

                for(; i<arrayLength;++i) {
                    *val = lastVal;
                    ++val;
                }
 
                qsParameter_setValue(p, value);
            }
            break;
        case QsValueType_double:
            {
                DASSERT(p->size % sizeof(double) == 0);
                arrayLength = p->size/sizeof(double);
                DASSERT(arrayLength);
                // We'll use stack memory for stuffing the values to and
                // then copy it to the parameter values.
                double value[arrayLength];
                // Set default first value:
                double lastVal = 0.0;
                double *val = value;
                // default value:
                for(; i<argc && i<arrayLength; ++i) {
                    char *end = 0;
                    *val = strtod(argv[i], &end);
                    if(argv[i] == end)
                        *val = lastVal;
                    else
                        lastVal = *val;

                    ++val;
                }

                for(; i<arrayLength;++i) {
                    *val = lastVal;
                    ++val;
                }

                qsParameter_setValue(p, value);
            }
            break;
        case QsValueType_uint64:
            {
                DASSERT(p->size % sizeof(uint64_t) == 0);
                arrayLength = p->size/sizeof(uint64_t);
                DASSERT(arrayLength);
                // We'll use stack memory for stuffing the values to and
                // then copy it to the parameter values.
                uint64_t value[arrayLength];
                // Set default first value:
                uint64_t lastVal = 0.0;
                uint64_t *val = value;
                // default value:
                for(; i<argc && i<arrayLength; ++i) {
                    char *end = 0;
                    *val = strtoul(argv[i], &end, 10);
                    if(argv[i] == end)
                        *val = lastVal;
                    else
                        lastVal = *val;
                    ++val;
                }

                for(; i<arrayLength;++i) {
                    *val = lastVal;
                    ++val;
                }
 
                qsParameter_setValue(p, value);
            }
            break;

        case QsValueType_string32:
            // A null terminated string, so it can only be visible 31
            // characters.
            {
                DASSERT(p->size % 32 == 0);
                arrayLength = p->size/sizeof(32);
                DASSERT(arrayLength);
                // Stack memory.  Oh boy. Oh shit.
                char value[arrayLength*32];
                memset(value, 0, arrayLength*32);

                for(; i<argc && i<arrayLength; ++i)
                    strncpy(value + i * 32, argv[i], 31);
                qsParameter_setValue(p, value);
            }
            break;

        default:
            ASSERT(0, "Write code for valueType=%d",
                    p->valueType);
            break;
    }

    return 0; // success
}


void qsParameter_setValue(struct QsParameter *p, const void *val) {

    NotWorkerThread();

    DASSERT(p);
    DASSERT(p->size);
    ASSERT(p->port.portType == QsPortType_setter, "Not a setter");
    ASSERT(val);

    DASSERT(p->port.block);
    struct QsGraph *graph = p->port.block->graph;
    DASSERT(graph);

    CHECK(pthread_mutex_lock(&graph->mutex));

    struct QsGroup *g = p->group;

    if(g) {
        if(g->getter) {
            // We cannot set it because a getter is in control.
            WARN("Setter \"%s:%s\" is connected to a getter, "
                    "cannot set value",
                    p->port.block->name, p->port.name);
            goto finish;
        }

        // Feed the value to the other setters in this group, g.
        PushGroupValue(g, val, p->size);
        goto finish;
    }

    // else: This is a setter with no connection yet
    struct QsJob *j = (void *)p + offsetof(struct QsSetter, job);

    if(!p->value) {

        qsJob_lock(j);
        p->value = malloc(p->size);
        ASSERT(p->value, "malloc(%zu) failed", p->size);
    } else
        qsJob_lock(j);

    waiting = true;

    // This is a little tricky.
    //
    if(busy || j->busy ||
            (j->inQueue && !(j->jobsBlock->threadPool->halt))) {
        // There is a thread pool worker thread for a block reading the
        // setter in setterWork() or there will be; otherwise the user is
        // a dumb-ass with a halted thread pool and is effectively setting
        // the setter to two values at once (or too quickly) and in the
        // that case not getting both values may be fine.
        CHECK(pthread_cond_wait(&cond, &mutex));
        waiting = false;
    }

    // Save the value.
    memcpy(p->value, val, p->size);

    qsJob_queueJob(j);

    // We use the setter readCount to show that we have a new value, for
    // in the case if the block was in the setter callback() while we are
    // here.
    ++((struct QsSetter *)p)->readCount;

    qsJob_unlock(j);


finish:

    CHECK(pthread_mutex_unlock(&graph->mutex));
}


bool qsParameter_isGetterGroup(struct QsParameter *p) {

    NotWorkerThread();

    DASSERT(p);
    DASSERT(p->port.portType == QsPortType_setter ||
            p->port.portType == QsPortType_getter);
    DASSERT(p->port.block);
    struct QsGraph *g = p->port.block->graph;
    DASSERT(g);

    CHECK(pthread_mutex_lock(&g->mutex));

    bool ret;

    if(!p->group)
        ret = false;
    else
        ret = (p->group->getter)?true:false;

    // God dam fucking C sucks.  Bug, just found, a double mutex lock
    // instead of a lock and unlock.  i.e. there was a fucking
    // CHECK(pthread_mutex_lock(&g->mutex)) here:
    CHECK(pthread_mutex_unlock(&g->mutex));
    // So {} scope limited mutexes like in C++ would never have that
    // happen.  Doing it in C makes inline function proliferation madness.
    // Like shitting on this page.  Well on the plus side it only took me
    // an hour to find and fix said bug.  So ya, one good thing about C++
    // over C.  Date: May 20 05:38:58 PM 2023
    //
    // Then again, all computer code looks like shit.

    return ret;
}


size_t qsParameter_getValue(const struct QsParameter *p,
        void *val, size_t size) {

    if(p->group)
        CHECK(pthread_mutex_lock(&p->group->mutex));
    else
        CHECK(pthread_mutex_lock(&mutex));

    if(!p->value) {
        size = 0;
        goto finish;
    }

    if(p->size < size)
        size = p->size;

    memcpy(val, p->value, size);

finish:

    if(p->group)
        CHECK(pthread_mutex_unlock(&p->group->mutex));
    else
        CHECK(pthread_mutex_unlock(&mutex));

    return size;
}


enum QsValueType qsParameter_getValueType(struct QsParameter *p) {

    DASSERT(p);
    DASSERT(p->valueType);
    // So long as the parameter exists this will never change.
    return p->valueType;
}


size_t qsParameter_getSize(struct QsParameter *p) {

    DASSERT(p);
    DASSERT(p->size);
    // So long as the parameter exists this will never change.
    return p->size;
}


bool qsParameter_canConnect(struct QsParameter *p1,
        struct QsParameter *p2, bool verbose) {

    DASSERT(p1);
    DASSERT(p2);

    if(p1->valueType != p2->valueType || p1->size != p2->size) {
        if(verbose)
            ERROR("Miss-matched parameter type and/or size");
        return false;
    }

    if(p1->port.portType == QsPortType_getter &&
        p2->port.portType == QsPortType_getter) {
        if(verbose)
            ERROR("Cannot connect to getters together");
        return false;
    }

    if(p2->port.portType == QsPortType_getter) {
        // This is redundant for calling from qsParameter_connect(), but
        // if this is called from else-where we need this.
        //
        // Switch p1 and p2 so that p1 is a getter.  If they are both
        // getters this will fail anyway.
        struct QsParameter *p = p2;
        p2 = p1;
        p1 = p;
    }

    // Are they already connected?
    if(p1->group && p1->group == p2->group) {
        // Yes.  They are already connected.
        // So answer is, they cannot be connected again.
        if(verbose)
            WARN("Parameters are already connected");
        return false;
    }

    // At this point they are not in the same connection group.
    if(p1->group && p2->group &&
            p1->group->getter &&
            p2->group->getter) {
        if(verbose)
            ERROR("Parameters are both in different "
                    "getter connection groups");
        return false;
    }

    if(p2->group && p1->port.portType == QsPortType_getter) {
        // At this point only p1 can be a getter.
        if(p2->group->getter) {
            if(verbose)
                ERROR("Cannot connect a getter to a getter group");
            return false;
        }
    }

    return true;
}


// This does not provide any multi-thread protection, that must be
// considered before this call.
//
static void
SetGroupValue(struct QsGroup *g, void *value) {

    DASSERT(g);
    DASSERT(g->size);

    // Did we have a value yet?
    if(!g->value) {
        DASSERT(g->writeCount == 0);
        // This is where the first value will go:
        g->value = g->values;
    }

    DASSERT(g->value < g->values + g->numValues*g->size);
    DASSERT(g->value >= g->values);

    memcpy(g->value, value, g->size);
    g->value += g->size;
    ++g->writeCount;

    if(g->value >= g->values + g->numValues*g->size)
        // Wrap back to the first element in the buffer.
        g->value = g->values;
}


static
uint32_t MergeGroups(struct QsGroup *g1, struct QsGroup *g2) {

    // For no reason, we keep g1 and destroy g2.  We had to pick one of
    // them.
    DASSERT(g1);
    DASSERT(g2);
    DASSERT(g1->size);
    DASSERT(g1->size == g2->size);
    DASSERT(!g2->getter);
    DASSERT(g1->sharedPeers);
    DASSERT(*g1->sharedPeers);
    DASSERT(g2->sharedPeers);
    DASSERT(*g2->sharedPeers);
    DASSERT(*g1->sharedPeers != *g2->sharedPeers);


    // Halt all thread pools that all the jobs in use in both groups.
    uint32_t numHalts = 0;

    for(struct QsJob **j = g1->sharedPeers; *j; ++j)
        numHalts += qsBlock_threadPoolHaltLock((*j)->jobsBlock);
    for(struct QsJob **j = g2->sharedPeers; *j; ++j)
        numHalts += qsBlock_threadPoolHaltLock((*j)->jobsBlock);


    // Reinitialize the all the jobs in group g2, and make they be in
    // group g1:
    //
    while(g2->sharedPeers) {
        struct QsJob *j = *g2->sharedPeers;
        struct QsJobsBlock *b = j->jobsBlock;
        // This will remove the job, j, from the array of jobs in group g2.
        //
        // When the last job, j, in the sharedPeers list is cleaned up
        // qsJob_cleanup(j) will set the value of g2->sharedPeers to 0,
        // with the pointer we gave the job in job_init().
        qsJob_cleanup(j);
        // This will add the job, j, to the other array of jobs in the
        // group g1.
        qsJob_init(j, b, Work, 0, &g1->sharedPeers);
        qsJob_addMutex(j, &g1->mutex);
        ++g1->numParameters;

        struct QsSetter *s = ((void *) j) -
            offsetof(struct QsSetter, job);
        // We need to mark this setter as having a value that
        // needs updating if there are values yet.
        s->parameter.value = 0;
        s->parameter.group = g1;

        if(g1->value)
            SetSetterToLastGroupValue((void *) s, g1);

        // The thread pools are halted so we should not need a lock.
        qsJob_queueJob(j);
    }

    // So now every job in the group is a peer to all jobs in this
    // group, g1.

    // We are done with this group, g2.
    DestroyGroup(g2, g2->size);

    return numHalts;
}


static
uint32_t AddParameterToGroup(struct QsGroup *g, struct QsParameter *p) {

    DASSERT(p);
    DASSERT(!p->group);
    DASSERT(g);
    DASSERT(g->numParameters);
    DASSERT(g->sharedPeers);
    DASSERT(*g->sharedPeers);

    uint32_t numHalts = 0;

    // Halt all thread pools that all the jobs used in this group
    // and this parameter, p.
    for(struct QsJob **jj = g->sharedPeers; *jj; ++jj)
        numHalts += qsBlock_threadPoolHaltLock((*jj)->jobsBlock);
    numHalts += qsBlock_threadPoolHaltLock((void *) p->port.block);


    struct QsJob *j = 0;
    if(p->port.portType == QsPortType_setter) {
        // This, p, parameter's job.
        j = ((void *) p) + offsetof(struct QsSetter, job);
        qsJob_cleanup(j);
    }
    // else the parameter is a getter, and getters do not have jobs.

    p->group = g;

    if(p->value) {

        // We discard this parameters value.
        DZMEM(p->value, p->size);
        free(p->value);
        p->value = 0;
    }

    if(j) {
        struct QsSetter *s = (void *) p;
        // We will reinitialize this:
        s->readCount = 0;

        qsJob_init(j, (void *) p->port.block, Work, 0, &g->sharedPeers);
        qsJob_addMutex(j, &g->mutex);

        if(g->value) {
            //
            // In general, there is no way to be sure how many values are
            // really present in the parameter group.  Integer counters
            // will always wrap through 0 when incremented with no limit.
            // There is at least one value, because g->value would not be
            // set if there was no values yet.  We just make the last
            // value be queued for this parameter, p.  Okay, we can
            // determine the number of values if the writeCount is not
            // near 0, but we do not want the user to ever get unexpected
            // behavior some of the time.  So, we make it act predictably
            // all of the time.  When a setter first connects, it will get
            // one value, the last value, if there are any values to
            // get.
            //
            // Set this parameters next value to the one behind the next
            // one that will be written.  That gives it one value to read.
            SetSetterToLastGroupValue((void *) p, g);

            qsJob_lock(j);
            qsJob_queueJob(j);
            qsJob_unlock(j);
        }

    } else {
        DASSERT(p->port.portType == QsPortType_getter);
        // This parameter, p, is a getter so it must be the first and only
        // getter in the group.  This should have already been checked
        // before this function call.
        DASSERT(!g->getter);
        g->getter = (void *) p;
    }

    ++g->numParameters;

    return numHalts;
}


static inline
uint32_t MakeNewGroup(struct QsParameter *p1, struct QsParameter *p2) {

    DASSERT(p1);
    DASSERT(!p1->group);
    DASSERT(p2);
    DASSERT(!p2->group);
    DASSERT(p1->size);
    DASSERT(p1->size == p2->size);
    // If there is a getter it's p1.
    DASSERT(p1->port.portType == QsPortType_setter ||
            p1->port.portType == QsPortType_getter);
    DASSERT(p2->port.portType == QsPortType_setter);
    DASSERT(p2->port.block->type & QS_TYPE_JOBS);

    uint32_t numHalts = 0;

    // j1 gets set if p1 is a setter.
    struct QsJob *j1 = 0;

    // j2 will not be from a getter.
    struct QsJob *j2 = &((struct QsSetter *)p2)->job;

    // Halt all thread pools that the parameters, p1 and p2, block's
    // interact with.
    //
    numHalts = qsBlock_threadPoolHaltLock((void *) p1->port.block);
    numHalts += qsBlock_threadPoolHaltLock((void *) p2->port.block);
    //
    if(p1->port.portType == QsPortType_setter) {
        // This, p1, parameter's job.
        j1 = ((void *) p1) + offsetof(struct QsSetter, job);
        qsJob_cleanup(j1);
    }
    // else the parameter, p1, is a getter, and getters do not have jobs.
    qsJob_cleanup(j2);


    struct QsGroup *g = calloc(1, sizeof(*g));
    ASSERT(g, "calloc(1,%zu) failed", sizeof(*g));
    CHECK(pthread_mutex_init(&g->mutex, 0));
    g->size = p1->size;
    p1->group = g;
    p2->group = g;

    g->values = calloc(parameterQueueLength, g->size);
    ASSERT(g->values, "calloc(%" PRIu32 ",%zu) failed",
            parameterQueueLength, g->size);
    // g->value = 0; // via calloc().
    g->numValues = parameterQueueLength;

    // TODO: Okay so we have-to address the problem of which parameter
    // values has priority, or should we discard one of the parameter
    // values.

    if(!j1) {
        DASSERT(p1->port.portType == QsPortType_getter);
        g->getter = (void *) p1; // group getter is p1.
    }

    if(p2->value) {
        SetGroupValue(g, p2->value);
        DZMEM(p2->value, p2->size);
        free(p2->value);
        p2->value = 0;
    }

    if(p1->value) {
        SetGroupValue(g, p1->value);
        DZMEM(p1->value, p1->size);
        free(p1->value);
        p1->value = 0;
    }


    if(j1) {
        qsJob_init(j1, (void *) p1->port.block, Work, 0, &g->sharedPeers);
        qsJob_addMutex(j1, &g->mutex);
    }
    qsJob_init(j2, (void *) p2->port.block, Work, 0, &g->sharedPeers);
    qsJob_addMutex(j2, &g->mutex);

    g->numParameters = 2;


    // Set the parameter for reading values.
    //
    if(g->value) {

        // Set values and queue setter jobs.

        if(j1) {
            // This p1 is a setter.
            SetSetterToLastGroupValue((void *) p1, g);
            qsJob_queueJob(j1);
        }

        // This p2 is a setter.
        SetSetterToLastGroupValue((void *) p2, g);
        qsJob_queueJob(j2);
    }

    return numHalts;
}


int qsParameter_connect(struct QsParameter *p1, struct QsParameter *p2) {

    NotWorkerThread();

    ASSERT(p1);
    ASSERT(p2);

    struct QsGraph *graph = p1->port.block->graph;
    DASSERT(graph);
    DASSERT(graph == p2->port.block->graph);

    CHECK(pthread_mutex_lock(&graph->mutex));


    if(p2->port.portType == QsPortType_getter) {
        // Switch p1 and p2 so that p1 is a getter.  If they are both
        // getters, qsParameter_canConnect() below will fail anyway.
        struct QsParameter *p = p2;
        p2 = p1;
        p1 = p;
    }

    int ret = 0;
    uint32_t numHalts = 0;

    // Are they already connected?
    if(p1->group && p1->group == p2->group) {
        // They are already connected.
        NOTICE("Control Parameters \"%s:%s\" and \"%s:%s\" "
                "are already connected",
                p1->port.block->name, p1->port.name,
                p2->port.block->name, p2->port.name);
        ret = 1; // fail positive.
        goto finish;
    }

    // We first search all for all the failure cases.
    if(!qsParameter_canConnect(p1, p2, true)) {
        ret = -1; // fail negative.
        goto finish;
    }


    // Mark the block which is doing this graph edit.  It could be a super
    // block or the graph (top block).  For the graph GetBlock() returns
    // 0.
    struct QsParentBlock *p = GetBlock(CB_DECLARE|CB_CONFIG, 0, 0);
    if(!p) p = (void *) graph;
    p1->port.parentBlock = p;
    p2->port.parentBlock = p;


    // Now we can connect them.

    if(p1->group && p2->group) {
        DASSERT(!(p1->group->getter && p2->group->getter));
        if(!p2->group->getter)
            numHalts = MergeGroups(p1->group, p2->group);
        else
             numHalts = MergeGroups(p2->group, p1->group);
    } else if(p1->group)
         numHalts = AddParameterToGroup(p1->group, p2);
    else if(p2->group)
         numHalts = AddParameterToGroup(p2->group, p1);
    else
         numHalts = MakeNewGroup(p1, p2);

finish:

    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(graph);


    CHECK(pthread_mutex_unlock(&graph->mutex));

    return ret;
}


int qsParameter_disconnect(struct QsParameter *p) {

    NotWorkerThread();

    ASSERT(p);

    struct QsGraph *graph = p->port.block->graph;
    DASSERT(graph);

    uint32_t numHalts = 0;
    int ret = 0;

    CHECK(pthread_mutex_lock(&graph->mutex));

    if(!p->group) {
        NOTICE("Control Parmeter \"%s:%s\" is already disconnected",
                p->port.block->name, p->port.name);
        ret = 1;
        goto finish;
    }

    numHalts += qsBlock_threadPoolHaltLock((void *) p->port.block);

    DisconnectParameter(p, p->group);

finish:

    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(graph);

    CHECK(pthread_mutex_unlock(&graph->mutex));

    return ret;
}


bool
qsParameterIsConnected(struct QsParameter *p) {

    ASSERT(pthread_getspecific(threadPoolKey),
            "Not called from a thread pool worker");

    return (p->group)?true:false;
}

