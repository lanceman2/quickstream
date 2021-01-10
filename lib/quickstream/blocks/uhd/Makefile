root := ../../../..

# It's not easy to make build options with quickbuild so we just "automate
# out" the "WITH_UHD make option" by this if switch which just cuts
# out just about the full make file if package uhd is not found.
#
ifeq ($(shell if pkg-config uhd --exists; then echo yes; fi),yes)

INSTALL_DIR = $(PREFIX)/lib/quickstream/plugins/filters/uhd




UHD_LIBS := $(shell pkg-config uhd --libs)
UHD_CXXFLAGS := $(shell pkg-config uhd --cflags)



tx.so_SOURCES := tx.cpp


txRun.so_SOURCES := txRun.cpp
txRun.so_LDFLAGS := $(UHD_LIBS)
txRun.so_CXXFLAGS := $(UHD_CXXFLAGS)



endif # ifeq ($(shell if pkg-config uhd ... bla bla bla.

include $(root)/quickbuild.make
