# Point of this directory is to encapsulate the source code for all
# the built-in block modules, and the code that helps access them.

# To add a built-in block module just add a C file to this directory
# or sub-directory in this directory, and then run "make" in ..

# listBuiltInBlocks.c will get compiled and linked with
# ../libquickstream.a and ../libquickstream.so
#
# The C source file that match $(wildcard [A-Z]*.c */[A-Z]*.c) or like
# thing will get compiled into and linked with
# ../libquickstream.a and ../libquickstream.so



libbuiltInBlocks.a_SOURCES := $(wildcard [A-Z]*.c */[A-Z]*.c)

listBuiltInBlocks.c: listBuiltInBlocks.bash libbuiltInBlocks.a listBuiltInBlocksPaths
	if ! ./listBuiltInBlocks.bash libbuiltInBlocks.a > $@ ; then rm $@ ; fi

# we use the program "nm" to get symbols from this archive
listBuiltInBlocksPaths: libbuiltInBlocks.a
	echo "$(patsubst %.c,%,$(libbuiltInBlocks.a_SOURCES))" > $@

BUILD_NO_INSTALL := listBuiltInBlocks.c listBuiltInBlocksPaths libbuiltInBlocks.a


include ../../quickbuild.make
