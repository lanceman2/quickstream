# 1.  build all [a-z]*.c as a block module
# 2.  build all [a-z]*.cpp as a block module
# 3.  build all [A-Z]*.c as a uninstalled test

root := ../../../../..

INSTALL_DIR = $(PREFIX)/lib/quickstream/blocks/controllers/tests

c_plugins := $(patsubst %.c, %, $(wildcard [a-z]*.c))

cpp_plugins := $(patsubst %.cpp, %, $(wildcard [a-z]*.cpp))

stdoutCPP.so_CPPFLAGS := -I$(root)/include


define makeSOURCES
$(1)$(2)_SOURCES := $(1)$(3)
endef


define ADDlib
$(1)_LDFLAGS := -L$(root)/lib -lquickstream -Wl,-rpath=\$$$$ORIGIN/$(root)/lib
endef

BUILD_NO_INSTALL := $(patsubst %.c, %, $(wildcard [A-Z]*.c))


$(foreach targ,$(c_plugins),$(eval $(call makeSOURCES,$(targ),.so,.c)))
$(foreach targ,$(cpp_plugins),$(eval $(call makeSOURCES,$(targ),.so,.cpp)))
$(foreach targ,$(BUILD_NO_INSTALL),$(eval $(call makeSOURCES,$(targ),,.c)))
$(foreach targ,$(BUILD_NO_INSTALL),$(eval $(call ADDlib,$(targ),)))


include $(root)/quickbuild.make
