# This directory has super block examples that are saved from running
# quickstreamGUI
#
# We wish to make these super block examples work without any by hand
# editing of the C source files, so that these examples are just taken
# just from what quickstreamGUI produces.  If you'd like to add an example
# here, do not create/edit it with anything but quickstreamGUI.


# build and install all [a-zA-Z]*.c and _[a-zA-Z].c as a block module
#
# install "other" files without a ".*" suffix


root := ../../../..


INSTALL_DIR = $(PREFIX)/lib/quickstream/blocks/examples

c_plugins := $(patsubst %.c, %, $(wildcard [0-9a-zA-Z]*.c) $(wildcard _[0-9a-zA-Z]*.c))


INSTALLED := $(filter-out %.so %.c Makefile qb_build, $(wildcard [0-9a-zA-Z]*))


define makeSOURCES
$(1)$(2)_SOURCES := $(1)$(3)
# This lets the #include <quickstream.h> and #include <quickstream_debug.h>
# be found when compiling these super blocks.
$(1)$(2)_CPPFLAGS := -I$(root)/include
endef


$(foreach targ,$(c_plugins),$(eval $(call makeSOURCES,$(targ),.so,.c)))



test:
	@echo "extra INSTALLED = $(INSTALLED)"


include $(root)/quickbuild.make
