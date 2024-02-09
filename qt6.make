
# apt install qt6-base-dev qt6-tools-dev qt6-wayland
#
# Get the QT6 (Qt version 6) specific compiler options (if we can).  We do
# this in a not so common way given that we are not married to QT; i.e. we
# are not using gmake (or Cmake) to make our make files and we do not want
# this quickstream software project to require QT, Cmake, et al.  This
# seems to work.  QT does not provide a pkg-config file so we cannot use
# pkg-config to find QT6 compiler options on Debian GNU/Linux 12
# (bookworm) as of February 2024.
#
# The scripts below will find this QT_INSTALL_LIBS
# (/usr/lib/x86_64-linux-gnu/ at this try) path and we see by running:
#
#       % ldd /usr/lib/x86_64-linux-gnu/libQt6Widgets.so.6
#
# it looks like all the libraries we may need.  We are just guessing at
# this point.  We see it is using libX11.so.6 at this time but hope that it
# will use libwayland-client.so (or like thing) in the future.  This test
# was on a Gnome desktop (Feb 2024) on Debian 12.  This shit was not in
# QT6 documentation anywhere that I could find, hence all this
# commenting is here; for when this breaks.  Running a test program shows
# that the libwayland-client.so was dynamically loaded (or something like
# that), cause Gnome was a wayland desktop at the time of the testing.
# I don't care (at this point), it works.


ifeq ($(WITHOUT_QT6),)

find := qtpaths6 --query

QT6_CFLAGS := $(sort $(shell $(find) QT_INSTALL_HEADERS))
ifeq ($(QT6_CFLAGS),)
$(error "Failed to get QT_INSTALL_HEADERS from $(find)")
endif
QT6_LDFLAGS := $(sort $(shell $(find) QT_INSTALL_LIBS))
ifeq ($(QT6_LDFLAGS),)
$(error "Failed to get QT_INSTALL_LIBS from $(find)")
endif
QT6_CFLAGS := -I$(QT6_CFLAGS) -DQT_NO_VERSION_TAGGING
# output of running: ldd libQt6Widgets.so
# shows that libQt6Widgets.so should already be linked with libQt6Core.so
# but this still fails to compile without the redundant "-lQt6Core"
# must be some kind of compiler voodoo on my system, at least.
#QT6_LDFLAGS := -L$(QT6_LDFLAGS) -lQt6Widgets -Wl,-rpath=$(QT6_LDFLAGS)
QT6_LDFLAGS := -L$(QT6_LDFLAGS) -lQt6Widgets -lQt6Core -Wl,-rpath=$(QT6_LDFLAGS)
#
undefine find

endif # ifeq ($(WITHOUT_QT6),)


# Spew what Qt6 stuff we have found
#$(warning QT6_CFLAGS="$(QT6_CFLAGS)" QT6_LDFLAGS="$(QT6_LDFLAGS)")

