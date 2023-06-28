# Note: not much happens with the files in this directory if
# the software package librtlsdr is not found via pkg-config.

root := ../../../..


# DIR should be the relative path to this directory from the top source
# directory.
DIR := lib/quickstream/blocks/rtlsdr


RTLSDR_CFLAGS := $(shell pkg-config --cflags librtlsdr)
ifeq ($(WITHOUT_RTLSDR),) 
RTLSDR_LDFLAGS := $(shell pkg-config --libs librtlsdr)
endif


# --------------------------------------------------------------------
ifeq ($(RTLSDR_LDFLAGS),)
# --------------------------------------------------------------------

# RTLSDR development libraries were not found so in this case we will
# not be building anything.
$(warning "Not building RTL-SDR blocks from $(DIR))

# --------------------------------------------------------------------
else
# --------------------------------------------------------------------


INSTALL_DIR = $(PREFIX)/$(DIR)


BUILD_NO_INSTALL := RawToASCII

RawToASCII_SOURCES := RawToASCII.c

_run.so_SOURCES := _run.c

_run.so_LDFLAGS :=\
 $(RTLSDR_LDFLAGS)\
 -Wl,-rpath=\$$ORIGIN/../../..

_run.so_CPPFLAGS := $(RTLSDR_CFLAGS)


rtlsdr.so_SOURCES := rtlsdr.c
rtlsdr.so_CFLAGS := $(RTLSDR_CFLAGS)

# --------------------------------------------------------------------
endif
# --------------------------------------------------------------------


include $(root)/quickbuild.make
