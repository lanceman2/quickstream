# This is a GNU make file that uses GNU make extensions

# This is the common make file that is included in all the block source
# directories.  This make file defines make targets and rules to make all
# the DSO (dynamic shared object) blocks in this quickstream package.  It
# skips building DSOs for the source to built-in blocks.  Built-in blocks
# are blocks that have their code compiled into libquickstream.so.
#
# The make file that includes this file must define root before including
# this file.



# root is the path to the build directory that included this file
# relative to the top quickstream source directory.

ifndef root
$(error "root was not defined before including $(lastword $(MAKEFILE_LIST))")
endif


# block_dir is the path to the block source directory relative to
# the directory that this make file is in.

this_dir := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

block_dir := $(patsubst $(this_dir)/%,%,$(realpath $(shell pwd)))

#$(warning "block_dir=$(block_dir)")

# $(root)/lib/builtInBlocks.txt is the list of all built-in blocks.
#
builtInBlocks := $(patsubst $(block_dir)/%,%,\
 $(addsuffix .c, $(filter $(block_dir)/%,\
 $(shell cat $(root)/lib/builtInBlocks.txt) | grep -ve '^\#')))

#$(warning "builtInBlocks=$(builtInBlocks)")


# Add the *.bi files to be installed in the particular block directory.
#
# Note: the *.bi files are created from running make in $(root)/lib/
INSTALLED := $(patsubst %.c,%.bi,$(builtInBlocks))


c_plugins :=\
 $(patsubst %.c, %, $(filter-out $(builtInBlocks),\
 $(wildcard [A-Za-z0-9]*.c) $(wildcard _[a-z0-9]*.c)))


# $(blocks) is set to non-zero length if we have any blocks to build,
# be they DSOs or built-in.
blocks := $(sort $(builtInBlocks) c_plugins)


# If there is a directory named qs_examples/ than we run make in there.
# qs_examples/ is special to the program $(root)/bin/quickstreamGUI which
# looks for the block use example flow graphs in that directory for the
# blocks source (install too) directory.  Source and installation
# directories are always same that is relative to the installation PREFIX
# directory.  That's a hard developer rule for quickstream: that is source
# directory tree is a mirror of the installed PREFIX directory tree, but
# without "all" the source files (just most of them).
ifndef SUBDIRS
SUBDIRS := $(shell if [ -d qs_examples ] ; then echo qs_examples ; fi)
endif



# We need this, CPPFLAGS, for building libquickstream.so auto-generated
# super blocks the have #include <quickstream.h> in them.  There are not
# many headers files in $(root)/include/.
CPPFLAGS += -I$(root)/include


BUILD_NO_INSTALL :=\
 $(patsubst %.c, %, $(filter-out $(builtInBlocks), $(wildcard _[A-Z]*.c)))

#$(warning "BUILD_NO_INSTALL=$(BUILD_NO_INSTALL)")



define makeSOURCES
$(1)$(2)_SOURCES := $(1)$(3)
endef


$(foreach targ,$(c_plugins),$(eval $(call makeSOURCES,$(targ),.so,.c)))
$(foreach targ,$(BUILD_NO_INSTALL),$(eval $(call makeSOURCES,$(targ),,.c)))



INSTALL_DIR = $(PREFIX)/lib/quickstream/blocks/$(block_dir)


include $(root)/quickbuild.make
