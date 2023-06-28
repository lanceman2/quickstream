#!/bin/bash

# this fails is there are no files [A-Z]*.c in .
#set -euo pipefail
# Because of the $(nm $1 | grep -e ' B ' | awk '{print $3}' | sort -u)

# Debug with
#set -xeu

set -eu


if [ -z "$1" ] ; then
    echo "Usage: $0 libbuiltInBlocks.a"
    exit 1
fi

# We get the C function names and global variables in the array symbols
# like so:
symbols=(
$(nm $1 | grep -e ' B ' | awk '{print $3}' | sort -u)
)

funcs=(
$(nm $1 | grep -e ' T ' | awk '{print $3}' | sort -u)
)



cat << EOF
// This is a generated file
// From running: $0 $*
// From directory: $PWD

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>

#include "../../include/quickstream.h"

#include "../debug.h"
#include "../Dictionary.h"

#include "../c-rbtree.h"
#include "../name.h"
#include "../threadPool.h"
#include "../block.h"
#include "../graph.h"
#include "../job.h"

#include "./builtInBlocks.h"



// These are not the correct function prototypes, but the correct
// function prototypes are not required at this point.  We figure
// that out later, and not in this file.
//
// There is a different guarantee for function pointers: Any function
// pointer can be converted to any other function pointer type and back
// again without loss of information.  We can even just look at them
// as void *
//
// You must convert back to the original pointer type before executing a
// call to avoid undefined behavior.


// Let the virtual address of these symbols be known:
EOF

for i in "${symbols[@]}" ; do
    cat << EOF
extern void *${i};
EOF
done

echo -e "\n// Let these functions be known:"
for i in "${funcs[@]}" ; do
    cat << EOF
extern void ${i}(void);
EOF
done


cat << EOF

struct QsDictionary *builtInBlocksFunctions = 0;


// Put these symbols and functions in a dictionary:
//
void GatherBuiltInBlocksFunctions(void) {

    DASSERT(builtInBlocksFunctions == 0);

    builtInBlocksFunctions = qsDictionaryCreate();
    ASSERT(builtInBlocksFunctions);

EOF

for i in "${symbols[@]}" "${funcs[@]}" ; do
    cat << EOF
    DASSERT(strlen("$i") + 1 <= MAX_BUILTIN_SYMBOL_LEN);
    ASSERT(0 == qsDictionaryInsert(builtInBlocksFunctions, "$i", $i, 0));
EOF
done


cat << EOF
}


const char * const qsBuiltInBlocks[] = {
EOF

for i in $(cat listBuiltInBlocksPaths); do
    echo "    \"${i}\","
done

cat << EOF
    0
};
EOF
