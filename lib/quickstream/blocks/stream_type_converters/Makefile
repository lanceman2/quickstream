
# 1.  build all [a-z]*.c and _[a-z].c as a block module
# 2.  build all [A-Z]*.c as a uninstalled test

root := ../../../..


INSTALL_DIR = $(PREFIX)/lib/quickstream/blocks/stream_type_converters

c_plugins := $(patsubst %.c, %, $(wildcard [a-z]*.c) $(wildcard _[a-z]*.c))


define makeSOURCES
$(1)$(2)_SOURCES := $(1)$(3)
endef


BUILD_NO_INSTALL := $(patsubst %.c, %, $(wildcard [A-Z]*.c))


$(foreach targ,$(c_plugins),$(eval $(call makeSOURCES,$(targ),.so,.c)))
$(foreach targ,$(BUILD_NO_INSTALL),$(eval $(call makeSOURCES,$(targ),,.c)))



include $(root)/quickbuild.make
