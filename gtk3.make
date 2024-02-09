
# apt install libgtk-3-dev
#
# Get the GTK3 specific compiler options if we can.  If GTK3 is
# installed where we can find it this way:
ifeq ($(WITHOUT_GTK3),)
GTK3_LDFLAGS := $(shell pkg-config --libs gtk+-3.0)
GTK3_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
endif

# Spew what GTK3 stuff we have found
$(warning GTK3_CFLAGS="$(GTK3_CFLAGS)" GTK3_LDFLAGS="$(GTK3_LDFLAGS)")

