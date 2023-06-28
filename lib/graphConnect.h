

static inline
struct QsDictionary *GetDictionary(const struct QsBlock *b,
        enum QsPortType pt) {

    DASSERT(b);
    struct QsPortDicts *ports;

    if(!(b->type & QS_TYPE_TOP)) {
        // This block is a simple of super block.
        struct QsModule *m = Module(b);
        DASSERT(m);
        ports = &m->ports;
    } else {
        // This block is a graph.
        DASSERT(b->type == QsBlockType_top);
        ports = &((struct QsGraph *) b)->ports;
    }

    switch(pt) {
        case QsPortType_setter:
            return ports->setters;
        case QsPortType_getter:
            return ports->getters;
        case QsPortType_input:
            return ports->inputs;
        case QsPortType_output:
            return ports->outputs;
        default:
            ASSERT(0);
            return 0;
    }

    ASSERT(0);
    return 0;
}


static inline
struct QsPort *_qsBlock_getPort(struct QsBlock *b,
        enum QsPortType pt, const char *pname) {

    DASSERT(b->graph);
    CHECK(pthread_mutex_lock(&b->graph->mutex));
    struct QsPort *p =
        qsDictionaryFind(GetDictionary(b, pt), pname);
    CHECK(pthread_mutex_unlock(&b->graph->mutex));
    return p;
}


static inline bool IsParameter(const struct QsPort *p) {

    return (p->portType == QsPortType_setter ||
            p->portType == QsPortType_getter);
}


static inline
enum QsPortType GetPortType(const char *type) {

    if(type[0] == 'S' || type[0] == 's')
        return QsPortType_setter;
    if(type[0] == 'G' || type[0] == 'g')
        return QsPortType_getter;
    if(type[0] == 'I' || type[0] == 'i')
        return QsPortType_input;
    if(type[0] == 'O' || type[0] == 'o')
        return QsPortType_output;
    ASSERT(0, "Bad port type \"%s\"", type);
    return 0;
}


static inline
char *GetPortTypeString(const struct QsPort *p) {

    DASSERT(p);

    switch(p->portType) {
        case QsPortType_input:
            return "i";
        case QsPortType_output:
            return "o";
        case QsPortType_setter:
            return "s";
        case QsPortType_getter:
            return "g";
        default:
            ASSERT(0, "Likely memory corruption");
            return 0;
        }
    return 0;
}
