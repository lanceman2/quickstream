#! This is a "GNU make" make file.

# If this directory is not recursed into from the directory above this we
# recurse up a directory if make was run in this directory.
ifeq ($(strip $(MAKELEVEL)),0)
# We do not try to build in .. if this is using GNU autotools.
ifeq ($(wildcard ../makefile)),)
SUBDIRS := ..
endif
endif


BUILD_NO_INSTALL :=\
 tests/tests\
 021_debug\
 180_Dictionary_test\
 165_DictionaryRemove_test\
 184_DictionaryRemove_test\
 190_DictionaryRemove_test\
 194_DictionaryDict_test\


# Just tests that debug.c is independent of other files,
# and so also does not depend on libquickstream.so
021_debug_SOURCES := 021_debug.c ../lib/debug.c


# This also tests that ../lib/Dictionary.c can be used
# without linking with libquickstream.so
#
# We wish to keep this Dictionary stuff just dependent on debug.c
165_DictionaryRemove_test_SOURCES := 165_DictionaryRemove_test.c ../lib/Dictionary.c ../lib/debug.c
180_Dictionary_test_SOURCES := 180_Dictionary_test.c ../lib/Dictionary.c ../lib/debug.c
184_DictionaryRemove_test_SOURCES := 184_DictionaryRemove_test.c ../lib/Dictionary.c ../lib/debug.c
190_DictionaryRemove_test_SOURCES := 190_DictionaryRemove_test.c ../lib/Dictionary.c ../lib/debug.c
194_DictionaryDict_test_SOURCES := 194_DictionaryDict_test.c ../lib/Dictionary.c ../lib/debug.c



ifeq ($(wildcard ../makefile)),)
# Needed only for testing with autotools building/testing.
tests/tests:
	ln -fs ../lib/quickstream/plugins/filters/tests/.libs tests/tests
endif

CLEANFILES := $(wildcard *.tmp)

CLEANERFILES := tests/tests


# We do not want 'test' to depend on build.  That would be intrusive and
# could create an infinite looping make in some cases.
test:
	@if [ ! -e ../include/quickstream/app.h ] ; then\
	    l="\n\n-------------------------------------------------------------------------\n\n";
	    echo -e "$$l    Consider running \`$(MAKE)' in $(shell readlink -m $(top_srcdir)) first$$l";\
	    exit 1;\
	fi
	./run_tests


include ../quickbuild.make
