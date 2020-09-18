
INSTALL_DIR = $(PREFIX)/bin


GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LDFLAGS := $(shell pkg-config --libs gtk+-3.0)
XML2_CFLAGS := $(shell pkg-config --cflags libxml-2.0)
XML2_LDFLAGS := $(shell pkg-config --libs libxml-2.0)

G_COMPILE_RES := glib-compile-resources


quickstream_SOURCES := quickstream.c
quickstream_LDFLAGS := -L../lib -lquickstream -Wl,-rpath=\$$ORIGIN/../lib

scribble_SOURCES := scribble.c
scribble_LDFLAGS := $(GTK_LDFLAGS) 
scribble_CPPFLAGS := $(GTK_CFLAGS)

ifeq ($(GTK_LDFLAGS),)
$(warning "Not building quickstreamBuilder")
else

CLEANFILES := qsb_res.c qsb_workArea_res.c


%_res.c: %_res.xml %_res.ui
	glib-compile-resources $< --target=$@ --sourcedir=. --generate-source



quickstreamBuilder_SOURCES :=\
 qsb.c\
 ../lib/debug.c\
 qsb_res.c\
 qsb_workArea_res.c

quickstreamBuilder_CPPFLAGS := $(GTK_CFLAGS) $(XML2_CFLAGS)
quickstreamBuilder_LDFLAGS := $(GTK_LDFLAGS) $(XML2_LDFLAGS)


endif



include ../quickbuild.make
