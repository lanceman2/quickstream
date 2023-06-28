#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>

#include "debug.h"



#define TMP_LEN  (64)


//
// CREDIT: We copied ideas for this from circular (ring) buffer idea from
// GNU radio.
//
// references:
//
// https://www.gnuradio.org/blog/buffers/
//
// https://github.com/gnuradio/gnuradio/blob/master/gnuradio-runtime/lib/vmcircbuf_mmap_shm_open.cc
//
// or if that link broke try a particular check-in:
//
// https://github.com/gnuradio/gnuradio/blob/01de47a/gnuradio-runtime/lib/vmcircbuf_mmap_shm_open.cc

// At first glance it seems a little heavy handed, but when you look at
// the buffer sizes used by the middle-ware SDR (software defined radio)
// drivers it seems reasonable, and I'd guess that the kernel driver
// buffers are about this size too.  We make the ring-buffer size be
// tuned by the block writer with the qsSetOutputMax() and
// qsSetInputMax() functions in the stream start() block callback
// functions.  The block writer needs to vary the ring-buffer size
// parameter until they get the kind of optimization they are looking for,
// using whatever measures they like.
//
// It doesn't appear that GNUradio varies the length of the stream ring
// buffers.  In quickstream the length of the ring buffers depends on the
// read and write promises set in qsSetOutputMax() and qsSetInputMax()
// functions in the stream start() block callback functions, and whither
// or not the buffer is a pass-through buffer or not.


// This makes a "ring buffer" that is made from two memory mappings, where
// the second memory mapping maps addresses at the end of the buffer are
// mapped back to memory at the start address.  It uses a shared memory
// mapped file, but only to force the memory to stay consistent between
// the two mappings.  The shared memory file is closed and unlinked before
// this function returns, so that the shared memory file may never be used
// for another purpose, accidentally or otherwise.   The parameters will
// be increased to the nearest page size (4096 bytes as of Sept 2019).
//
// After "len" and "overhang" are increased to the nearest page size the
// buffer will looks like:
//
//
//   "start"                                  "mark"                  "end"
//      |------------------ len -----------------|------ overhang ------|
//
//
//
//  where the memory starting at "mark" is mapped to the memory starting
//  at "start".
//
//  "len" should be the longest amount of memory that needs to be stored
//  at any given time.
//
//  "overhang" should be the maximum length that is accessed in a single
//  action, be it read or write.
//
//

// TODO:
//
// Since mmap()ing and munmap() this memory is an expensive part of the
// stream startup and stopping, we need to consider making a circular
// buffer allocator by putting the buffers in a (red/black?) tree with a
// key that is the sum of mapLength and overhangLength.  Maybe a searching
// a singly linked list would be much faster than all the mmap()ing and
// munmap() system calls.  Or we could put the list of circular buffers in
// an ordered array so we can have O(log) speed search.  Or maybe just not
// sweat it because the kernel will be caching these pages for us anyway,
// so the cost gets smaller when we cycle the starting and stopping.
//



static size_t pagesize = 0;

// Make *len be the nearest multiple of pagesize.
//
static inline void bumpSize(size_t *len)
{
    DASSERT(pagesize);

    if((*len) > (size_t) pagesize)
    {
        if((*len) % pagesize)
            *len += pagesize - (*len) % pagesize;
    }
    else
        *len = pagesize;
}


// Sets len to the next pagesize.
// Sets overhang to the next pagesize.
//
// Returns a pointer to the start of the first mapping.
//
void *makeRingBuffer(size_t *len, size_t *overhang)
{
    DASSERT(len);
    DASSERT(overhang);

    if(!pagesize) {
        pagesize = getpagesize();
        // Lets see if this ever changes:
        DASSERT(pagesize == 4*1024);
    }

    // This is not thread safe.  We expect that this is only
    // called by the main thread.
    //
    static uint32_t segmentCount = 0;

    DASSERT((*len) >= (*overhang));

    bumpSize(len);
    bumpSize(overhang);

    char tmp[TMP_LEN];
    uint8_t *x;
    int fd;

    snprintf(tmp, TMP_LEN, "/qs_ringbuffer_%" PRIu32 "_%d",
            segmentCount, getpid());


    // It's all just a crap ton of system calls that can't fail hence
    // ASSERT().


    // Using shm_open(), instead of open(), incurs less overhead in use.
    // reference:
    //   https://stackoverflow.com/questions/24875257/why-use-shm-open
    //
    ASSERT((fd = shm_open(tmp, O_RDWR|O_CREAT|O_EXCL, S_IRUSR | S_IWUSR))
            > -1, "shm_open(\"%s\",,) failed", tmp);

    ASSERT(ftruncate(fd, (*len) + (*overhang)) == 0, "ftruncate() failed");

    ASSERT(MAP_FAILED != (x = (uint8_t *) mmap(0, (*len) + (*overhang),
            PROT_WRITE|PROT_READ, MAP_SHARED,
            fd,  0/*file offset*/)), "mmap() failed");

    // Make a hole of size "overhang" in the mapping.  If we did not make
    // the original mapping larger than we'd have no way to make the
    // second mapping be next to the first mapping.
    ASSERT(0 == munmap(x + (*len), *overhang));

    // Fill the hole with the starting memory of the last mapping using
    // the start of the file to make it be at the start.
    //
    // I'd guess that this next mapping could end up not next to the first
    // mapping, but we'll have a failed assertion if it does not.
    ASSERT(x + (*len) == (uint8_t *) mmap(x + (*len), *overhang,
            PROT_WRITE|PROT_READ, MAP_SHARED,
            fd,  0/*file offset*/),
            "mmap() failed to return the address we wanted");

    ASSERT(close(fd) == 0);

    ASSERT(shm_unlink(tmp) == 0, "shm_unlink(\"%s\") failed", tmp);

    return x;
}


//  x        is the start of the first mapping.
//  len      is the length in bytes of the first mapping
//  overhang is the is the length in bytes of the overhang mapping.
//
void freeRingBuffer(void *x, size_t len, size_t overhang)
{
    DASSERT(x);
    DASSERT(len);
    DASSERT(overhang);
    DASSERT(len%pagesize == 0);
    DASSERT(overhang%pagesize == 0);

    DZMEM(x, len);

    ASSERT(0 == munmap(x, len));
    ASSERT(0 == munmap((uint8_t *) x + len, overhang));
}
