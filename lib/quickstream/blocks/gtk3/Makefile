# Note: not much happens with the files in this directory if
# GTK 3 is not found.

root := ../../../..



GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
ifeq ($(WITHOUT_GTK3),) 
GTK_LDFLAGS := $(shell pkg-config --libs gtk+-3.0)
endif


# --------------------------------------------------------------------
ifeq ($(GTK_LDFLAGS),)
# --------------------------------------------------------------------

# GTK3 development libraries were not found so in this case we will
# not be building or installing anything.
$(warning NOTICE Not building GTK3 blocks from $(shell pwd))

# We still need the generic make targets:
include $(root)/quickbuild.make

# --------------------------------------------------------------------
else
# --------------------------------------------------------------------

# Looks like every thing we compile (CPP) needs the GTK_CFLAGS
CPPFLAGS := $(GTK_CFLAGS)

_run.so_LDFLAGS :=\
 $(GTK_LDFLAGS)\
 -L../../.. -lqsGtk_init\
 -Wl,-rpath=\$$ORIGIN/../../..

Scribble_LDFLAGS := $(GTK_LDFLAGS)


# We now use the generic block building make rules from:
include $(root)/lib/quickstream/blocks/common.make


# --------------------------------------------------------------------
endif
# --------------------------------------------------------------------

