# Note: not much happens with the files in this directory if
# GTK 3 is not found.

root := ../../../..


# define GTK3_LDFLAGS and GTK3_CFLAGS, or not.
include $(root)/gtk3.make


# --------------------------------------------------------------------
ifeq ($(GTK3_LDFLAGS),)
# --------------------------------------------------------------------

# GTK3 development libraries were not found so in this case we will
# not be building or installing anything.
$(warning NOTICE Not building GTK3 blocks from $(shell pwd))

# We still need the generic make targets:
include $(root)/quickbuild.make

# --------------------------------------------------------------------
else
# --------------------------------------------------------------------

# Looks like every thing we compile (CPP) needs the GTK3_CFLAGS
CPPFLAGS := $(GTK3_CFLAGS)

_run.so_LDFLAGS :=\
 $(GTK3_LDFLAGS)\
 -L$(root)/lib -lqsGtk_init\
 -Wl,-rpath=\$$ORIGIN/$(root)/lib

Scribble_LDFLAGS := $(GTK3_LDFLAGS)


# We now use the generic block building make rules from:
include $(root)/lib/quickstream/blocks/common.make


# --------------------------------------------------------------------
endif
# --------------------------------------------------------------------

