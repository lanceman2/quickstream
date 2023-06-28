
INSTALL_DIR = $(PREFIX)/bin


GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
ifeq ($(WITHOUT_GTK3),)
GTK_LDFLAGS := $(shell pkg-config --libs gtk+-3.0)
endif


common_sources :=\
 getOpt.c\
 quickstream_RunCommand.c\
 quickstream_help.c\
 quickstream_RunInterpreter.c


quickstream_SOURCES :=\
 quickstream.c\
 $(common_sources)


quickstream_interpreter_SOURCES :=\
 quickstream_interpreter.c\
 $(common_sources)


quickstream_LDFLAGS :=\
 -L../lib -lquickstream\
 -Wl,-rpath=\$$ORIGIN/../lib -ldl


quickstream_interpreter_LDFLAGS :=\
 $(quickstream_LDFLAGS)


# --------------------------------------------------------------------
ifeq ($(GTK_LDFLAGS),)
# --------------------------------------------------------------------

$(warning Not building quickstreamGUI, The quickstream GUI program.)

# --------------------------------------------------------------------
else
# --------------------------------------------------------------------

BUILD_NO_INSTALL := scribble treeView treeView_directoryTest gtk_cells


gtk_cells_SOURCES :=\
 gtk_cells.c\
 ../lib/debug.c
gtk_cells_LDFLAGS :=\
 $(GTK_LDFLAGS)
gtk_cells_CPPFLAGS := $(GTK_CFLAGS)


quickstreamGUI_SOURCES :=\
 quickstreamGUI.c\
 qsg_window.c\
 qsg_tab.c\
 qsg_layout.c\
 qsg_treeView.c\
 qsg_CreateTreeView.c\
 qsg_treeViewPopupMenu.c\
 qsg_block.c\
 qsg_cssProvider.c\
 qsg_terminal.c\
 qsg_drawLines.c\
 qsg_quickstream.c\
 qsg_connection.c\
 qsg_terminalPopupMenu.c\
 qsg_cursor.c\
 qsg_blockPopupMenu.c\
 qsg_blockConfigWindow.c\
 qsg_util.c\
 qsg_tabPopupMenu.c\
 qsg_assignBlocksToThreadsWindow.c\
 qsg_createThreadPoolsWindow.c\
 qsg_buttonBar.c

quickstreamGUI_LDFLAGS :=\
 $(GTK_LDFLAGS)\
 -L../lib -lqsGtk_init\
 -L../lib -lquickstream\
 -Wl,-rpath=\$$ORIGIN/../lib\
 -ldl -lm

quickstreamGUI_CPPFLAGS := $(GTK_CFLAGS)


scribble_SOURCES := scribble.c
scribble_LDFLAGS := $(GTK_LDFLAGS)
scribble_CPPFLAGS := $(GTK_CFLAGS)

gtk_overlay_SOURCES := gtk_overlay.c
gtk_overlay_LDFLAGS := $(GTK_LDFLAGS)
gtk_overlay_CPPFLAGS := $(GTK_CFLAGS)


treeView_SOURCES := treeView.c
treeView_LDFLAGS := $(GTK_LDFLAGS)
treeView_CPPFLAGS := $(GTK_CFLAGS)

treeView_directoryTest_SOURCES :=\
 treeView_directoryTest.c\
 ../lib/debug.c\
 treeView_test.c
treeView_directoryTest_LDFLAGS := $(GTK_LDFLAGS)
treeView_directoryTest_CPPFLAGS := $(GTK_CFLAGS)

# --------------------------------------------------------------------
endif
# --------------------------------------------------------------------



include ../quickbuild.make
