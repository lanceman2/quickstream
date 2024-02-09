# Note: not much happens with the files in this directory if
# the software package librtlsdr is not found via pkg-config.
#
# https://github.com/librtlsdr/librtlsdr
#
# apt install librtlsdr-dev

root := ../../../..



RTLSDR_CFLAGS := $(shell pkg-config --cflags librtlsdr)
ifeq ($(WITHOUT_GTK3),) 
RTLSDR_LDFLAGS := $(shell pkg-config --libs librtlsdr)
endif


# --------------------------------------------------------------------
ifeq ($(RTLSDR_LDFLAGS),)
# --------------------------------------------------------------------

# wsServer development libraries were not found so in this case we will
# not be building or installing anything.
$(warning NOTICE Not building wsServer blocks from $(shell pwd)")

# We still need the generic make targets:
include $(root)/quickbuild.make

# --------------------------------------------------------------------
else
# --------------------------------------------------------------------

# Looks like every thing we compile (CPP) needs the RTLSDR_CFLAGS
CPPFLAGS := $(RTLSDR_CFLAGS)

_run.so_LDFLAGS :=\
 $(RTLSDR_LDFLAGS)\
 -L../../.. -lqsGtk_init\
 -Wl,-rpath=\$$ORIGIN/../../..


# We now use the generic block building make rules from:
include $(root)/lib/quickstream/blocks/common.make


# --------------------------------------------------------------------
endif
# --------------------------------------------------------------------

