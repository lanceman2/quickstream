
root := ../../../..


# DIR should be the relative path to this directory from the top source
# directory.
DIR := lib/quickstream/blocks/gnuradio-3.10.1

INSTALL_DIR = $(PREFIX)/$(DIR)


BUILD_NO_INSTALL := u8tofloat QT_GNUradio_test.so


# Additionally install
INSTALLED := QT_GUI_Sink


CPPFLAGS := -I$(root)/include


u8tofloat_SOURCES := u8tofloat.c

QT_GNUradio_sink.so_SOURCES := QT_GNUradio_sink.c

QT_GNUradio_test.so_SOURCES := QT_GNUradio_test.c


include $(root)/quickbuild.make
