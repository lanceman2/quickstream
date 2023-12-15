# Note: not much happens with the files in this directory if
# the wsServer software is not found.
#
# https://github.com/Theldus/wsServer


root := ../../../..


# DIR should be the relative path to this directory from the top source
# directory.
DIR := lib/quickstream/blocks/wsServer


# WSS stands for wsServer

WSS_CFLAGS := $(shell pkg-config --cflags wsserver)
ifeq ($(WITHOUT_WSS),) 
WSS_LDFLAGS := $(shell pkg-config --libs wsserver)
endif


# --------------------------------------------------------------------
ifeq ($(WSS_LDFLAGS),)
# --------------------------------------------------------------------

# WSS development libraries were not found so in this case we will
# not be building anything.
$(warning "Not building wsServer block(s) from $(DIR))

# --------------------------------------------------------------------
else
# --------------------------------------------------------------------


INSTALL_DIR = $(PREFIX)/$(DIR)


_run.so_SOURCES := _run.c
_run.so_LDFLAGS :=\
 $(WSS_LDFLAGS)\
 -Wl,-rpath=\$$ORIGIN/../../..
_run.so_CPPFLAGS := $(WSS_CFLAGS)

websocketServer.so_SOURCES := websocketServer.c
websocketServer.so_CFLAGS := $(WSS_CFLAGS)


# --------------------------------------------------------------------
endif
# --------------------------------------------------------------------


include $(root)/quickbuild.make
