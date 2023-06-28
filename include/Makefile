# We broke the libquistream.so header file into a few smaller source files
# and we build another version of it for running doxygen on.

INSTALL_DIR = $(PREFIX)/include


BUILD := quickstream.h quickstream_debug.h quickstream_mprintf.h
BUILD_NO_INSTALL := quickstream_expanded.h


quickstream_expanded.h: header.h block.h builder.h runner.h footer.h
	echo -e "// This is a generated file\n" > $@
	cat $^ >> $@

# TODO: note this assumes we are using GCC
quickstream.h: quickstream_expanded.h
	echo -e "// This is a generated file\n" > $@
	$(CC) -Werror -Wall -fpreprocessed -dD -E -P $< >> $@

quickstream_debug.h: ../lib/debug.h
	cp $^ $@

quickstream_mprintf.h: ../lib/mprintf.h
	cp $^ $@




include ../quickbuild.make
