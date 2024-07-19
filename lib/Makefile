SUBDIRS := pkgconfig


INSTALL_DIR = $(PREFIX)/lib



# libquickstream.so is just libquickstream.a as a dynamic shared object
# (DSO) library so it has just libquickstream.a as a source but with
# special linker options in libquickstream.so_LDFLAGS; so ya it
# has no .c source files:
libquickstream.so_SOURCES :=
#
# running: nm --extern-only libquickstream.so will show just the symbols
# that library users can access.  Without the
# -Wl,--retain-symbols-file=retain-symbols.txt we get more symbols that
# libquickstream.so users can access.  We want less symbols accessible to
# the library user.
#
# QS_EXPORT is used to let the library header ../include/quickbuild.h know
# that they need to export symbols for the library.
#
libquickstream.so_CFLAGS :=\
 -fvisibility=hidden\
 -DEXPORT=QS_EXPORT\
 -DQS_EXPORT="__attribute__((visibility(\"default\"))) extern"
#
libquickstream.so_LDFLAGS :=\
 -Wl,--whole-archive\
 libquickstream.a\
 -Wl,--no-whole-archive\
 -lpthread -ldl -lrt\
 -Wl,--retain-symbols-file=retain-symbols.txt
#
libquickstream.so: retain-symbols.txt libquickstream.a

# running: nm --extern-only libquickstream.so will show just the symbols
# that users can access when linking with this library.  Without the
# -Wl,--retain-symbols-file=retain-symbols.txt we get more symbols that
# libquickstream.so users can access, which we do not want.

builtInBlocks := $(shell cat builtInBlocks.txt | grep -ve '^\#')
builtInBlocks_bi := $(addprefix quickstream/blocks/,$(addsuffix .bi, $(builtInBlocks)))


# We install little place holder files in place of built-in blocks.
#
# Note: we will not install these files from running "make install" here;
# we will install these *.bi files from the corresponding block directory.
BUILD_NO_INSTALL := $(builtInBlocks_bi)

# Rule used to make a built-in block marker file:
# Note: it does not depend on anything as seen by this make.
#
# In the future we could put more information in these *.bi files;
# maybe keep these files in the source and installed, and remove
# the file builtInBlocks.txt.
%.bi:
	echo "# This is a generated file for the quickstream built-in block $(patsubst quickstream/blocks/%.bi,%,$@)" > $@


builtInBlocks_c :=\
 $(addprefix quickstream/blocks/,$(addsuffix .c, $(builtInBlocks)))


# Why build a library archive (libquickstream.a)?  We need it to make test
# programs that link with symbols that are not exposed in
# libquickstream.so and yet we only build the object files once; that is
# we build libquickstream.a which we use to build libquickstream.so.
libquickstream.a_SOURCES :=\
 debug.c\
 Dictionary.c\
 lib_constructor.c\
 name.c\
 threadPool.c\
 graph.c\
 block.c\
 job.c\
 threadPoolHalt.c\
 c-rbtree.c\
 block_threadPools.c\
 qsGraph_createBlock.c\
 FindFullPath.c\
 parameter.c\
 signalThread.c\
 graphCommand.c\
 qsGraph_connect.c\
 qsGraph_disconnect.c\
 graphPrint.c\
 config.c\
 parse.c\
 stream.c\
 interBlockJob.c\
 qsGraph_start.c\
 qsGraph_stop.c\
 qsBlock.c\
 streamWork.c\
 mmapRingBuffer.c\
 qsOpenDLHandle.c\
 qsGetMemory.c\
 qsGraph_flatten.c\
 qsGraph_saveSuperBlock.c\
 qsGraph_save.c\
 epoll.c\
 metaData.c\
 qsBlock_printPorts.c\
 listBuiltInBlocks.c\
 $(builtInBlocks_c)

libbuiltInBlocks.a_SOURCES := $(builtInBlocks_c)

BUILD_NO_INSTALL += listBuiltInBlocks.c libbuiltInBlocks.a

# compiling c-rbtree.c requires this:
libquickstream.a_CPPFLAGS := -I.

listBuiltInBlocks.c: listBuiltInBlocks.bash libbuiltInBlocks.a
	if ! ./listBuiltInBlocks.bash libbuiltInBlocks.a > $@ ; then rm $@ ; fi



# define GTK3_CFLAGS and GTK3_LDFLAGS, or not.
include ../gtk3.make

# --------------------------------------------------------------------
ifeq ($(GTK3_LDFLAGS),)
# --------------------------------------------------------------------

# GTK3 development libraries were not found so in this case we will
# not be building anything.
$(warning NOTICE Not building GTK3 blocks from $(DIR))

# --------------------------------------------------------------------
else
# --------------------------------------------------------------------

libqsGtk_init.so_SOURCES := qsGtk_init.c
libqsGtk_init.so_CPPFLAGS := -DBUILD_LIB $(GTK3_CFLAGS)
libqsGtk_init.so_LDFLAGS :=\
 -Wl,--retain-symbols-file=qsGtk_init-symbols.txt
libqsGtk_init.so: qsGtk_init-symbols.txt

# --------------------------------------------------------------------
endif
# --------------------------------------------------------------------



# define QT6_CFLAGS and QT6_LDFLAGS, or not.
include ../qt6.make

# --------------------------------------------------------------------
ifeq ($(QT6_LDFLAGS),)
# --------------------------------------------------------------------

$(warning NOTICE Not building Qt6 blocks from $(DIR))

# --------------------------------------------------------------------
else
# --------------------------------------------------------------------

libqsQtApp.so_SOURCES := qsQtApp.cpp
libqsQtApp.so_CPPFLAGS := -DBUILD_LIB $(QT6_CFLAGS)
libqsQtApp.so_LDFLAGS := -Wl,--retain-symbols-file=qsQtApp-symbols.txt\
 $(QT6_LDFLAGS)
libqsQtApp.so: qsQtApp-symbols.txt

qsQtApp_test_SOURCES := qsQtApp_test.cpp debug.c
# I do not know why I need to include the QT6_LDFLAGS here and in the
# building of libqsQtApp.so above; but qsQtApp_test will not link without
# this redundancy.
qsQtApp_test_LDFLAGS := $(QT6_LDFLAGS) -L../lib -lqsQtApp -Wl,-rpath=\$$ORIGIN
qsQtApp_test_CPPFLAGS := $(QT6_CFLAGS)
qsQtApp_test: libqsQtApp.so

BUILD_NO_INSTALL += qsQtApp_test

# --------------------------------------------------------------------
endif
# --------------------------------------------------------------------



include ../quickbuild.make
