
INSTALL_DIR = $(PREFIX)/lib


libquickstream.so_CFLAGS :=\
 -fvisibility=hidden\
 -DBUILD_LIB

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
 mmapRingBuffer.c\
 blockFlowAPI.c\
 blockStartAPI.c\
 blockDeclareAPI.c\
 parameterForEach.c\
 parameterAPIUtil.c

libquickstream.so_LDFLAGS :=\
 -Wl,--retain-symbols-file=retain-symbols.txt\
 -lpthread -ldl -lrt

# See the symbols retained with the command:
#
#    readelf --symbols libquickstream.so
# or
#    nm --extern-only libquickstream.so
#
# need to use commands like:
#
# nm -D libquickstream.so | grep " T " | awk '{print $3}' | egrep '^qs'


include ../quickbuild.make
