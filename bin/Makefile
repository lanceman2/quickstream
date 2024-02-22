# This is a "GNU make" make file.
#
# One of the "quickstream" design objectives was as to have as few
# software package dependences as possible, even at the expense of
# portability.  If quickstream can easily and only build (with these make
# file) on a GNU/Linux and maybe a Free BSD system, than fine.  The
# porting to MS (microsoft) windoz and MacOS will most likely be done with
# a more native MS windoz (and MacOS like) build system like MS visual
# studio (and whatever on MacOS), so there is very little point in
# making these "Makefile" make files portable.  We also do not want of
# marry this quickstream software with QT or GTK.  Both of which have a
# very "we are the center-of-the-universe" design; with GTK so much so, as
# to not have a destructor-paired function to gtk_init().  That is, if you
# initialize the GTK main loop thingy your process is committed for the
# life of the process, and the GTK main loop code will bleed system
# resources if you try to remove the GTK code at runtime.  This GTK
# behavior is undocumented.  I call it "not robust" by design in the UNIX
# sense of the word, given that they do not document this behavior, as
# would be required for it to be robust.
#
# I expect that QT may also have similar "center-of-the-universe"-like
# behavior; but that should be expected given QT seems to be designed with
# great concern for portability, and consequently has a ton of portability
# wrappers.  We wish to keep the quickstream-QT relationship to just the
# QT widgets, and not get in bed with QT.  I expect that the "code bloat"
# from the use of QT to be much greater than with using GTK.
#
# We know that if you "buy into" using and writing quickstream blocks to
# do your development you are losing a lot of flexibility.  This "block
# coding" programming paradigm is very very restricting from the get-go.
# We hope not to extend that pain any farther than is needed and we
# encourage you to not make quickstream blocks if the block coding
# programming paradigm does not suit your needs.


INSTALL_DIR = $(PREFIX)/bin

BUILD_NO_INSTALL :=


# define GTK3_CFLAGS and GTK3_LDFLAGS, or not.
include ../gtk3.make


# define QT6_CFLAGS and QT6_LDFLAGS, or not.
include ../qt6.make


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
ifeq ($(QT6_LDFLAGS),)
# --------------------------------------------------------------------

$(warning NOTICE Not building Qt version 6 based quickstream programs)

# --------------------------------------------------------------------
else # ifeq ($(QT6_LDFLAGS),)
# --------------------------------------------------------------------
BUILD_NO_INSTALL += qt6_test qt6_treeview_test

qt6_2app_test_SOURCES := qt6_2app_test.cpp ../lib/debug.c
qt6_2app_test_LDFLAGS :=$(QT6_LDFLAGS)
qt6_2app_test_CPPFLAGS := $(QT6_CFLAGS)


qt6_treeview_test_SOURCES :=\
 qt6_treeview_test.cpp\
 qsQt_treeview_moc.cpp
qt6_treeview_test_LDFLAGS :=\
 $(QT6_LDFLAGS)\
 -L../lib -lqsQtApp\
 -L../lib -lQt6Gui\
 -L../lib -lquickstream\
 -Wl,-rpath=\$$ORIGIN/../lib

qt6_treeview_test_CPPFLAGS := $(QT6_CFLAGS)


quickstreamQt_SOURCES :=\
 quickstreamQt.cpp\
 qsQt_window.cpp\
 qsQt_treeview_moc.cpp

CLEANFILES := qsQt_treeview_moc.cpp qt6_treeview_test_moc.cpp


# ../lib/libqsQtApp.so is linked to the QT6 libraries already;
# so we do not need a $(QT6_LDFLAGS) in this; unless compiler voodoo
# requires it:
quickstreamQt_LDFLAGS :=\
 $(QT6_LDFLAGS)\
 -L../lib -lqsQtApp\
 -L../lib -lQt6Gui\
 -L../lib -lquickstream\
 -Wl,-rpath=\$$ORIGIN/../lib\
 -ldl -lm

quickstreamQt_CPPFLAGS := $(QT6_CFLAGS)


%_moc.cpp: %.cpp
	cat $< > $@
	$(QT6_LIBEXECSC)/moc -i $< >> $@



# --------------------------------------------------------------------
endif # ifeq ($(QT6_LDFLAGS),)
# --------------------------------------------------------------------





# --------------------------------------------------------------------
ifeq ($(GTK3_LDFLAGS),)
# --------------------------------------------------------------------

$(warning NOTICE Not building GTK3 based quickstreamGUI program.)

# --------------------------------------------------------------------
else # ifeq ($(GTK3_LDFLAGS),)
# --------------------------------------------------------------------

BUILD_NO_INSTALL += scribble treeView treeView_directoryTest gtk_cells


gtk_cells_SOURCES :=\
 gtk_cells.c\
 ../lib/debug.c
gtk_cells_LDFLAGS :=\
 $(GTK3_LDFLAGS)
gtk_cells_CPPFLAGS := $(GTK3_CFLAGS)


quickstreamGUI_attach_SOURCES :=\
 quickstreamGUI_attach.c\
 ../lib/debug.c

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
 $(GTK3_LDFLAGS)\
 -L../lib -lqsGtk_init\
 -L../lib -lquickstream\
 -Wl,-rpath=\$$ORIGIN/../lib\
 -ldl -lm

quickstreamGUI_CPPFLAGS := $(GTK3_CFLAGS)


scribble_SOURCES := scribble.c
scribble_LDFLAGS := $(GTK3_LDFLAGS)
scribble_CPPFLAGS := $(GTK3_CFLAGS)

gtk_overlay_SOURCES := gtk_overlay.c
gtk_overlay_LDFLAGS := $(GTK3_LDFLAGS)
gtk_overlay_CPPFLAGS := $(GTK3_CFLAGS)


treeView_SOURCES := treeView.c
treeView_LDFLAGS := $(GTK3_LDFLAGS)
treeView_CPPFLAGS := $(GTK3_CFLAGS)

treeView_directoryTest_SOURCES :=\
 treeView_directoryTest.c\
 ../lib/debug.c\
 treeView_test.c
treeView_directoryTest_LDFLAGS := $(GTK3_LDFLAGS)
treeView_directoryTest_CPPFLAGS := $(GTK3_CFLAGS)

# --------------------------------------------------------------------
endif # ifeq ($(GTK3_LDFLAGS),)
# --------------------------------------------------------------------



include ../quickbuild.make
