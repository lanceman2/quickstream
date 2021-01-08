root := ../../../..

# It's not easy to make build options with quickbuild so we just "automate
# out" the "WITH_UHD make option" by this if switch which just cuts
# out just about the full make file if package uhd is not found.
#
ifeq ($(shell if pkg-config uhd --exists; then echo yes; fi),yes)


#  build all [a-z]*.cpp as a quickstream filter module


INSTALL_DIR = $(PREFIX)/lib/quickstream/plugins/filters/uhd


cpp_plugins := $(patsubst %.cpp, %, $(wildcard [a-z]*.cpp))

UHD_LIBS := $(shell pkg-config uhd --libs)
UHD_CXXFLAGS := $(shell pkg-config uhd --cflags)


define makeSOURCES
$(1)$(2)_SOURCES := $(1)$(3)
$(1)$(2)_LDFLAGS := $(UHD_LIBS)
$(1)$(2)_CXXFLAGS := $(UHD_CXXFLAGS)
endef



$(foreach targ,$(cpp_plugins),$(eval $(call makeSOURCES,$(targ),.so,.cpp)))


endif # ifeq ($(shell if pkg-config uhd ... bla bla bla.

include $(root)/quickbuild.make
