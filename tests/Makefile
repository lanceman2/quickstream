#! This is a "GNU make" make file.

# Adding a test that is a bash script does not require editing this file.


# If this directory is not recursed into from the directory above this we
# recurse up a directory if make was run in this directory.
ifeq ($(strip $(MAKELEVEL)),0)
# We do not try to run make in .. if this is using GNU autotools.
ifeq ($(wildcard ../makefile)),)
SUBDIRS := ..
endif
endif


# Either library should work.
#
# Static linking:
QS_LIBA := ../lib/libquickstream.a
# Dynamic linking:
QS_LIB := -L../lib -lquickstream -Wl,-rpath=\$$ORIGIN/../lib
#
# QS_LIB := ../lib/libquickstream.so # works too but then
# programs will not run with the relative path not correct,
# without setting env LD_LIBRARY_PATH for the path to
# the directory that contains libquickstream.so.


BUILD_NO_INSTALL :=\
 021_debug\
 040_qsGraph_create\
 041_qsGraph_create_static\
 047_qsGraph_noDestroy\
 052_qsGraphExit\
 053_qsGraph_waitCommand\
 malloc_leak\
 atomic_test\
 fail\
 success\
 010_skipTest\
 itimer_test\
 setjmp\
 getline\
 mod\
 pthread_once\
 returnStatus\
 c-rbtree_00\
 structAddressMadness\
 060_job\
 062_job_noLibDir\
 065_job\
 068_job_1t\
 070_job_2t\
 072_job_3t\
 074_job_4t\
 080_job_parallel\
 083_job\
 085_jobSharedPeers\
 086_job_multiThreadPool\
 088_job_multiThreadPool\
 090_job_multiThreadPool\
 093_job_multiThreadPool\
 095_job_multiThreadPool\
 097_job_multiThreadPool\
 100_oneBlock\
 101_oneBlock\
 104_varyMaxThreads\
 107_varyMaxThreads\
 111_blocksChangeThreadPool\
 115_blocksChangeThreadPool\
 120_removeBlock\
 123_removeBlock\
 127_removeJobs\
 129_removeJobs\
 131_addMutexes\
 133_addMutexes\
 471_execBlock.so\
 801_qs_dlopen\
 _zerosInFile


structAddressMadness_SOURCES := structAddressMadness.c

returnStatus_SOURCES := returnStatus.c

fail_SOURCES := fail.c
success_SOURCES := success.c
010_skipTest_SOURCES := 010_skipTest.c

pthread_once_SOURCES := pthread_once.c ../lib/debug.c
pthread_once_LDFLAGS := -lpthread -lrt

atomic_test_SOURCES := atomic_test.c ../lib/debug.c
atomic_test_LDFLAGS := -lpthread -lrt

itimer_test_SOURCES := itimer_test.c ../lib/debug.c
setjmp_SOURCES := setjmp.c ../lib/debug.c
getline_SOURCES := getline.c
mod_SOURCES := mod.c

# Just tests that debug.c is independent of other files,
# and so also does not depend on libquickstream.so
021_debug_SOURCES := 021_debug.c ../lib/debug.c

471_execBlock.so_SOURCES := 471_execBlock.c ../lib/debug.c
471_execBlock.so_LDFLAGS := -Wl,-e,main

040_qsGraph_create_SOURCES := 040_qsGraph_create.c
040_qsGraph_create_LDFLAGS := $(QS_LIB)

# Also test linking to libquickstream statically.
041_qsGraph_create_static_SOURCES := 040_qsGraph_create.c
041_qsGraph_create_static_LDFLAGS := $(QS_LIBA)
041_qsGraph_create_static: $(QS_LIBA)

044_qsGraph_create_many_SOURCES := 044_qsGraph_create_many.c
044_qsGraph_create_many_LDFLAGS := $(QS_LIB)

047_qsGraph_noDestroy_SOURCES := 047_qsGraph_noDestroy.c
047_qsGraph_noDestroy_LDFLAGS := $(QS_LIB)

052_qsGraphExit_SOURCES := 052_qsGraphExit.c
052_qsGraphExit_LDFLAGS := $(QS_LIB)

053_qsGraph_waitCommand_SOURCES := 053_qsGraph_waitCommand.c
053_qsGraph_waitCommand_LDFLAGS := $(QS_LIB)


# 060_job uses internal parts of the quickstream library code
# so we must link it statically with libquickstream.a
#
#    Run: bash%  ldd 040_qsGraph_create
#    and         ldd 041_qsGraph_create_static
#
060_job_SOURCES := 060_job.c
060_job_LDFLAGS := $(QS_LIBA)
060_job: $(QS_LIBA)

# This, 062_job_noLibDir, is testing having a exec real path that is not
# in this directory.  It runs the 060_job test but from a different binary
# path; which makes the statically linked program unable to find the
# library and block modules directories.  It's just a case that we'd like
# to still be able to run okay, knowing that loading core blocks may not
# work.
062_job_noLibDir: 060_job
	mkdir -p subDir
	cp $< subDir/$@
	ln -sf subDir/$@ $@

# All the "job" tests need to be statically linked so that they have more
# symbol access.  They're testing "library internals".
065_job_SOURCES := 065_job.c
065_job_LDFLAGS := $(QS_LIBA)
065_job: $(QS_LIBA)

# 1 thread
068_job_1t_SOURCES := 068_job_nt.c
068_job_1t_LDFLAGS := $(QS_LIBA)
068_job_1t: $(QS_LIBA)
068_job_1t_CPPFLAGS := -DMAXTHREADS=1

# 2 threads
070_job_2t_SOURCES := 068_job_nt.c
070_job_2t_LDFLAGS := $(QS_LIBA)
070_job_2t: $(QS_LIBA)
070_job_2t_CPPFLAGS := -DMAXTHREADS=2

# 3 threads
072_job_3t_SOURCES := 068_job_nt.c
072_job_3t_LDFLAGS := $(QS_LIBA)
072_job_3t: $(QS_LIBA)
072_job_3t_CPPFLAGS := -DMAXTHREADS=3

# 4 threads
074_job_4t_SOURCES := 068_job_nt.c
074_job_4t_LDFLAGS := $(QS_LIBA)
074_job_4t: $(QS_LIBA)
074_job_4t_CPPFLAGS := -DMAXTHREADS=4


080_job_parallel_SOURCES := 080_job_parallel.c
080_job_parallel_LDFLAGS := $(QS_LIBA)
080_job_parallel: $(QS_LIBA)

083_job_SOURCES := 083_job.c
083_job_LDFLAGS := $(QS_LIBA)
083_job: $(QS_LIBA)

085_jobSharedPeers_SOURCES := 085_jobSharedPeers.c
085_jobSharedPeers_LDFLAGS := $(QS_LIBA)
085_jobSharedPeers: $(QS_LIBA)

086_job_multiThreadPool_SOURCES := 086_job_multiThreadPool.c
086_job_multiThreadPool_LDFLAGS := $(QS_LIBA)
086_job_multiThreadPool: $(QS_LIBA)

088_job_multiThreadPool_SOURCES := 086_job_multiThreadPool.c
088_job_multiThreadPool_LDFLAGS := $(QS_LIBA)
088_job_multiThreadPool: $(QS_LIBA)
088_job_multiThreadPool_CPPFLAGS := -DREMOVE_TP


090_job_multiThreadPool_SOURCES := 090_job_multiThreadPool.c
090_job_multiThreadPool_LDFLAGS := $(QS_LIBA)
090_job_multiThreadPool: $(QS_LIBA)

093_job_multiThreadPool_SOURCES := 090_job_multiThreadPool.c
093_job_multiThreadPool_LDFLAGS := $(QS_LIBA)
093_job_multiThreadPool: $(QS_LIBA)
093_job_multiThreadPool_CPPFLAGS := -DREMOVE_TP

095_job_multiThreadPool_SOURCES := 090_job_multiThreadPool.c
095_job_multiThreadPool_LDFLAGS := $(QS_LIBA)
095_job_multiThreadPool: $(QS_LIBA)
095_job_multiThreadPool_CPPFLAGS := -DNO_QUEUE

097_job_multiThreadPool_SOURCES := 090_job_multiThreadPool.c
097_job_multiThreadPool_LDFLAGS := $(QS_LIBA)
097_job_multiThreadPool: $(QS_LIBA)
097_job_multiThreadPool_CPPFLAGS := -DREMOVE_TP -DNO_QUEUE

100_oneBlock_SOURCES := 100_oneBlock.c
100_oneBlock_LDFLAGS := $(QS_LIBA)
100_oneBlock: $(QS_LIBA)

101_oneBlock_SOURCES := 100_oneBlock.c
101_oneBlock_LDFLAGS := $(QS_LIBA)
101_oneBlock: $(QS_LIBA)
101_oneBlock_CPPFLAGS := -DNO_QUEUE



104_varyMaxThreads_SOURCES := 104_varyMaxThreads.c
104_varyMaxThreads_LDFLAGS := $(QS_LIBA)
104_varyMaxThreads: $(QS_LIBA)

107_varyMaxThreads_SOURCES := 104_varyMaxThreads.c
107_varyMaxThreads_LDFLAGS := $(QS_LIBA)
107_varyMaxThreads: $(QS_LIBA)
107_varyMaxThreads_CPPFLAGS := -DNO_QUEUE

111_blocksChangeThreadPool_SOURCES := 111_blocksChangeThreadPool.c
111_blocksChangeThreadPool_LDFLAGS := $(QS_LIBA)
111_blocksChangeThreadPool: $(QS_LIBA)

115_blocksChangeThreadPool_SOURCES := 111_blocksChangeThreadPool.c
115_blocksChangeThreadPool_LDFLAGS := $(QS_LIBA)
115_blocksChangeThreadPool: $(QS_LIBA)
115_blocksChangeThreadPool_CPPFLAGS := -DNO_QUEUE

120_removeBlock_SOURCES := 120_removeBlock.c
120_removeBlock_LDFLAGS := $(QS_LIBA)
120_removeBlock: $(QS_LIBA)

123_removeBlock_SOURCES := 120_removeBlock.c
123_removeBlock_LDFLAGS := $(QS_LIBA)
123_removeBlock: $(QS_LIBA)
123_removeBlock_CPPFLAGS := -DNO_QUEUE


127_removeJobs_SOURCES := 127_removeJobs.c
127_removeJobs_LDFLAGS := $(QS_LIBA)
127_removeJobs: $(QS_LIBA)

129_removeJobs_SOURCES := 127_removeJobs.c
129_removeJobs_LDFLAGS := $(QS_LIBA)
129_removeJobs: $(QS_LIBA)
129_removeJobs_CPPFLAGS := -DNO_QUEUE

131_addMutexes_SOURCES := 131_addMutexes.c
131_addMutexes_LDFLAGS := $(QS_LIBA)
131_addMutexes: $(QS_LIBA)

133_addMutexes_SOURCES := 131_addMutexes.c
133_addMutexes_LDFLAGS := $(QS_LIBA)
133_addMutexes: $(QS_LIBA)
133_addMutexes_CPPFLAGS := -DNO_QUEUE

SearchArray_SOURCES := SearchArray.c


801_qs_dlopen_SOURCES :=\
 801_qs_dlopen.c\
 ../lib/debug.c


_zerosInFile_SOURCES :=\
 _zerosInFile.c



malloc_leak_SOURCES := malloc_leak.c


c-rbtree_00_SOURCES :=\
 c-rbtree_00.c\
 ../lib/c-rbtree.c\
 ../lib/debug.c
c-rbtree_00_CPPFLAGS := -I../lib


ifeq ($(wildcard ../makefile)),)
# Needed only for testing with autotools building/testing.
tests/tests:
	ln -fs ../lib/quickstream/plugins/filters/tests/.libs tests/tests
endif


include ../qt6.make


# --------------------------------------------------------------------
ifeq ($(QT6_LDFLAGS),)
# --------------------------------------------------------------------

$(warning NOTICE Not building Qt version 6 based quickstream test programs)

# --------------------------------------------------------------------
else # ifeq ($(QT6_LDFLAGS),)
# --------------------------------------------------------------------
BUILD_NO_INSTALL += qt6_app_test

qt6_app_test_SOURCES := qt6_app_test.cpp
qt6_app_test_LDFLAGS :=$(QT6_LDFLAGS)
qt6_app_test_CPPFLAGS := $(QT6_CFLAGS)

# --------------------------------------------------------------------
endif # ifeq ($(QT6_LDFLAGS),)
# --------------------------------------------------------------------




# Remove temporary files that are generated by running tests:
CLEANFILES := $(wildcard *.tmp) $(wildcard data_*)

CLEANERFILES := tests/tests

CLEANDIRS := subDir


# We do not want 'test' to depend on build.  That would be intrusive and
# could create an infinite looping make in some cases, but we can at least
# check that libquickstream.so is built.  This warning will not preempt
# the failed compile attempts before this, but it's better than no
# warning.
test:
	@if [ ! -e ../lib/libquickstream.so ] ; then\
	    l="\n\n-------------------------------------------------------------------------\n\n";
	    echo -e "$$l    Consider running \`$(MAKE)' in $(shell readlink -m $(top_srcdir)) first$$l";\
	    exit 1;\
	fi
	./run_tests


include ../quickbuild.make
