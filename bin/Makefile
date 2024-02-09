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


# apt install libgtk-3-dev qt6-wayland
#
# Get the GTK3 specific compiler options (if we can):
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
ifeq ($(WITHOUT_GTK3),)
GTK_LDFLAGS := $(shell pkg-config --libs gtk+-3.0)
endif


# apt install qt6-base-dev qt6-tools-dev
#
# Get the QT6 (Qt version 6) specific compiler options (if we can).  We do
# this in a not so common way given that we are not married to QT; i.e. we
# are not using gmake (or Cmake) to make our make files and we do not want
# this quickstream software project to require QT, Cmake, et al.  This
# seems to work.  QT does not provide a pkg-config file so we cannot use
# pkg-config to find QT6 compiler options on Debian GNU/Linux 12
# (bookworm).
#
# The scripts below will find this QT_INSTALL_LIBS
# (/usr/lib/x86_64-linux-gnu/ at this try) path and we see by running:
#
#       % ldd /usr/lib/x86_64-linux-gnu/libQt6Widgets.so.6
#
# it looks like all the libraries we may need.  We are just guessing at
# this point.  We see it is using libX11.so.6 at this time but hope that it
# will use libwayland-client.so (or like thing) in the future.  This test
# was on a gnome desktop (Feb 2024) on Debian 12.  This shit was not in
# QT6 documentation anywhere that I could find, hence all this
# commenting is here; for when this breaks.
#
ifeq ($(WITHOUT_QT6),)
QT6_PATHS := $(shell qtpaths6 --query)
endif
#
ifneq ($(QT6_PATHS),)
QT6_CFLAGS := $(sort $(shell qtpaths6 --query QT_INSTALL_HEADERS))
ifeq ($(QT6_CFLAGS),)
$(error "Failed to get QT_INSTALL_HEADERS from qtpaths6 --query")
endif
QT6_LDFLAGS := $(sort $(shell qtpaths6 --query QT_INSTALL_LIBS))
ifeq ($(QT6_LDFLAGS),)
$(error "Failed to get QT_INSTALL_LIBS from qtpaths6 --query")
endif
QT6_CFLAGS := -I$(QT6_CFLAGS) -DQT_NO_VERSION_TAGGING
# output of running: ldd libQt6Widgets.so
# shows that libQt6Widgets.so should already be linked with libQt6Core.so
# but this still fails to compile without the redundant "-lQt6Core"
# must be some kind of compiler voodoo on my system, at least.
#QT6_LDFLAGS := -L$(QT6_LDFLAGS) -lQt6Widgets -Wl,-rpath=$(QT6_LDFLAGS)
QT6_LDFLAGS := -L$(QT6_LDFLAGS) -lQt6Widgets -lQt6Core -Wl,-rpath=$(QT6_LDFLAGS) 
endif
#
# Spew what Qt6 stuff we have found
#$(warning QT6_CFLAGS="$(QT6_CFLAGS)" QT6_LDFLAGS="$(QT6_LDFLAGS)")



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
BUILD_NO_INSTALL += qt6_test

qt6_2app_test_SOURCES := qt6_2app_test.cpp ../lib/debug.c
qt6_2app_test_LDFLAGS := $(QT6_LDFLAGS)
qt6_2app_test_CPPFLAGS := $(QT6_CFLAGS)

# --------------------------------------------------------------------
endif # ifeq ($(QT6_LDFLAGS),)
# --------------------------------------------------------------------





# --------------------------------------------------------------------
ifeq ($(GTK_LDFLAGS),)
# --------------------------------------------------------------------

$(warning NOTICE Not building GTK3 based quickstreamGUI program.)

# --------------------------------------------------------------------
else # ifeq ($(GTK_LDFLAGS),)
# --------------------------------------------------------------------

BUILD_NO_INSTALL += scribble treeView treeView_directoryTest gtk_cells


gtk_cells_SOURCES :=\
 gtk_cells.c\
 ../lib/debug.c
gtk_cells_LDFLAGS :=\
 $(GTK_LDFLAGS)
gtk_cells_CPPFLAGS := $(GTK_CFLAGS)


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
endif # ifeq ($(GTK_LDFLAGS),)
# --------------------------------------------------------------------



include ../quickbuild.make
