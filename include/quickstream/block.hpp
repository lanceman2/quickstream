#ifndef __qsblock_hpp__
#define __qsblock_hpp__



#ifndef __cplusplus
#error "This is not a C++ compiler"
#endif

#ifndef __qsblock_h__
#error "You must include quickstream/block.h before this"
#endif



#define _QS_FLUSH_TO_FLOW         ((int) 0xffa5e761)
#define _QS_FLOW_DOES_NOT_EXIST   ((int) 0xff62eeca)


// Here's the C++ API

/** block module base class that is inherited by C++ block
 module classes

 \headerfile block.h "quickstream/block.h"

 This is the base class that you can inherit to make a C++ quickstream
 block module.   You can use the CPP macro QS_LOAD_BLOCK_MODULE() to
 load your C++ object.

 Below is an example C++ quickstream block module:
 \include stdoutCPP.cpp
 */
class QsBlock {

    public:

        // We don't need a constructor for this base class.

        /**
         * \details \copydoc CBlockAPI::construct()
         */
        virtual int construct(void) {
            return 0;
        };

        /**
         * \details \copydoc CBlockAPI::flow()
         */
        virtual int flow(void *inBuffers[], const size_t inLens[],
            uint32_t numInPorts, uint32_t numOutPorts) {
            return _QS_FLOW_DOES_NOT_EXIST;
        };

        /**
         * \details \copydoc CBlockAPI::flush()
         */
        virtual int flush(void *inBuffers[], const size_t inLens[],
            uint32_t numInPorts, uint32_t numOutPorts) {
            return _QS_FLUSH_TO_FLOW;
        };

        /**
         * \details \copydoc CBlockAPI::start()
         */
        virtual int start(uint32_t numInPorts, uint32_t numOutPorts) {
            return 1;
        };

        /**
         * \details \copydoc CBlockAPI::stop()
         */
        virtual int stop(uint32_t numInPorts, uint32_t numOutPorts) {
            return 1;
        };

        /**
         * \details \copydoc CBlockAPI::destroy()
         */
        virtual ~QsBlock(void) { };
};


#ifndef DOXYGEN_SHOULD_SKIP_THIS

static class QsBlock *f = 0;


/** C++ loader CPP (C preprocessor) macro to create your C++ block object
 *
 * The C++ loader CPP (C preprocessor) macro that will create a C++
 * superclass object from the quickstream module loader function
 * qsGraphLoadBlock() of the from the \ref quickstream_program.
 */
#define QS_LOAD_BLOCK_MODULE(ClassName) \
    extern "C" {\
    int declare(void) {\
        f = new ClassName;\
        return 0;\
    }\
}


extern "C" {

// We wrap all the C++ class QsBlock base methods with C wrapper
// functions.  And so the libquickstream block module loader will
// just call these C wrappers, and that's how it works.
//
// Note: We do not provide a C++ wrapper API for the super blocks,
// at least not in here.
//
// TODO: It'd be nice to remove the flow() and flush() methods in
// declare(), if they are not overridden by the C++ block writer.
//

int construct(void) {
    return f->construct();
}


int flow(void *buffers[], const size_t lens[],
        uint32_t numInPorts, uint32_t numOutPorts) {

    int ret = f->flow(buffers, lens, numInPorts, numOutPorts);

    if(ret == _QS_FLOW_DOES_NOT_EXIST)
        return 1;

    return ret;
}


int flush(void *buffers[], const size_t lens[],
        uint32_t numInPorts, uint32_t numOutPorts) {

    int ret = f->flush(buffers, lens, numInPorts, numOutPorts);

    if(ret == _QS_FLUSH_TO_FLOW)
        return flow(buffers, lens, numInPorts, numOutPorts);

    return ret;
}


int start(uint32_t numInPorts, uint32_t numOutPorts) {

    return f->start(numInPorts, numOutPorts);
}

int stop(uint32_t numInPorts, uint32_t numOutPorts) {

    return f->stop(numInPorts, numOutPorts);
}

void destroy(void) {

    delete f;
    f = 0;
}


} // extern "C"


#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS



#endif // #ifndef __qsblock_hpp__
