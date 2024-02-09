# Note: not much happens with the files in this directory if
# wsServer is not found.
#
# https://github.com/Theldus/wsServer/

root := ../../../..


WSS_CFLAGS := $(shell pkg-config --cflags wsserver)
ifeq ($(WITHOUT_WSS),) 
WSS_LDFLAGS := $(shell pkg-config --libs wsserver)
endif


# --------------------------------------------------------------------
ifeq ($(WSS_LDFLAGS),)
# --------------------------------------------------------------------

# wsServer development libraries were not found so in this case we will
# not be building or installing anything.
$(warning NOTICE Not building wsServer blocks from $(shell pwd)")

# We still need the generic make targets:
include $(root)/quickbuild.make

# --------------------------------------------------------------------
else
# --------------------------------------------------------------------

# Looks like every thing we compile (CPP) needs the WSS_CFLAGS
CPPFLAGS := $(WSS_CFLAGS)

_run.so_LDFLAGS :=\
 $(WSS_LDFLAGS)\
 -L../../.. -lqsGtk_init\
 -Wl,-rpath=\$$ORIGIN/../../..


# We now use the generic block building make rules from:
include $(root)/lib/quickstream/blocks/common.make


# --------------------------------------------------------------------
endif
# --------------------------------------------------------------------

