# 1.  build all [a-z]*.c as a block module
# 2.  build all [a-z]*.cpp as a block module
# 3.  build all [A-Z]*.c as a uninstalled test

root := ../../../..

INSTALL_DIR = $(PREFIX)/lib/quickstream/misc/test_blocks

c_plugins := $(sort $(patsubst %.c, %, $(wildcard [a-z]*.c) $(wildcard _[a-z]*.c)) empty)

cpp_plugins := $(patsubst %.cpp, %, $(wildcard [a-z]*.cpp))


define makeSOURCES
$(1)$(2)_SOURCES := $(1)$(3)
endef


# If it turns out we need to link with libquickstream.so we'll have to
# change the order in which make runs in subdirectories of lib/; that is
# this directory need to make after ../../../../lib/.
#
define ADDlib
$(1)_LDFLAGS := -L$(root)/lib -lquickstream -Wl,-rpath=\$$$$ORIGIN/$(root)/lib
endef

BUILD_NO_INSTALL := $(patsubst %.c, %, $(wildcard [A-Z]*.c))


empty.c:
	touch $@

CLEANFILES := empty.c


$(foreach targ,$(c_plugins),$(eval $(call makeSOURCES,$(targ),.so,.c)))
$(foreach targ,$(cpp_plugins),$(eval $(call makeSOURCES,$(targ),.so,.cpp)))
$(foreach targ,$(BUILD_NO_INSTALL),$(eval $(call makeSOURCES,$(targ),,.c)))
#$(foreach targ,$(BUILD_NO_INSTALL),$(eval $(call ADDlib,$(targ),)))


Sequence_SOURCES += $(root)/lib/debug.c


include $(root)/quickbuild.make
