
INSTALL_DIR = $(PREFIX)/lib

libquickstream.so_SOURCES :=\
 debug.c\
 Dictionary.c\
 block.c\
 parameter.c\
 graph.c\
 GetPluginPath.c\
 trigger.c\
 run.c\
 threadPool.c\
 stream.c\
 graphPrint.c\
 ringBuffers.c\
 mmapRingBuffer.c


libquickstream.so_LDFLAGS := -lpthread -ldl -lrt
# TODO: need to add something like: -export-symbols-regex '^qs'


include ../quickbuild.make
