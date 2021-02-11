
INSTALL_DIR = $(PREFIX)/bin


GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LDFLAGS := $(shell pkg-config --libs gtk+-3.0)

G_COMPILE_RES := glib-compile-resources


quickstream_SOURCES :=\
 quickstream.c\
 getOpt.c\
 quickstream_help.c\
 quickstream_action.c\
 quickstream_interpreter.c

quickstream_LDFLAGS := -L../lib -lquickstream -Wl,-rpath=\$$ORIGIN/../lib -ldl



ifeq ($(GTK_LDFLAGS),)
# The quickstream program does not depend on GTK so in this case the
# quickstream program will be built.
$(warning "Not building quickstreamBuilder")
else

BUILD_NO_INSTALL := scribble treeView treeView1

scribble_SOURCES := scribble.c
scribble_LDFLAGS := $(GTK_LDFLAGS)
scribble_CPPFLAGS := $(GTK_CFLAGS)

treeView_SOURCES := treeView.c
treeView_LDFLAGS := $(GTK_LDFLAGS)
treeView_CPPFLAGS := $(GTK_CFLAGS)

treeView_directoryTest_SOURCES :=\
 treeView_directoryTest.c\
 ../lib/debug.c\
 qsb_treeView.c
treeView_directoryTest_LDFLAGS := $(GTK_LDFLAGS)
treeView_directoryTest_CPPFLAGS := $(GTK_CFLAGS)


CLEANFILES := qsb_res.c qsb_workArea_res.c qsb_popup_res.c


%_res.c: %_res.xml %_res.ui
	glib-compile-resources $< --target=$@ --sourcedir=. --generate-source



quickstreamBuilder_SOURCES :=\
 qsb.c\
 qsb_res.c\
 qsb_workArea_res.c\
 qsb_popup_res.c\
 qsb_treeView.c\
 qsb_BlockSelector.c\
 qsb_block.c\
 qsb_CSS.c

quickstreamBuilder_CPPFLAGS := $(GTK_CFLAGS)
quickstreamBuilder_LDFLAGS := $(quickstream_LDFLAGS) $(GTK_LDFLAGS) -lm

endif



include ../quickbuild.make
