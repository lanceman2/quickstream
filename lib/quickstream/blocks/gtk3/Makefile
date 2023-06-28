# Note: not much happens with the files in this directory if
# GTK 3 is not found.


root := ../../../..


# DIR should be the relative path to this directory from the top source
# directory.
DIR := lib/quickstream/blocks/gtk3


GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
ifeq ($(WITHOUT_GTK3),) 
GTK_LDFLAGS := $(shell pkg-config --libs gtk+-3.0)
endif


# --------------------------------------------------------------------
ifeq ($(GTK_LDFLAGS),)
# --------------------------------------------------------------------

# GTK3 development libraries were not found so in this case we will
# not be building anything.
$(warning "Not building GTK3 blocks from $(DIR))

# --------------------------------------------------------------------
else
# --------------------------------------------------------------------


INSTALL_DIR = $(PREFIX)/$(DIR)


BUILD_NO_INSTALL := Scribble


_run.so_SOURCES := _run.c
_run.so_LDFLAGS :=\
 $(GTK_LDFLAGS)\
 -L../../.. -lqsGtk_init\
 -Wl,-rpath=\$$ORIGIN/../../..
_run.so_CPPFLAGS := $(GTK_CFLAGS)

base.so_SOURCES := base.c
base.so_CFLAGS := $(GTK_CFLAGS)

slider.so_SOURCES := slider.c
slider.so_CFLAGS := $(GTK_CFLAGS)

button.so_SOURCES := button.c
button.so_CFLAGS := $(GTK_CFLAGS)

text.so_SOURCES := text.c
text.so_CFLAGS := $(GTK_CFLAGS)


Scribble_SOURCES := Scribble.c
Scribble_LDFLAGS := $(GTK_LDFLAGS)
Scribble_CPPFLAGS := $(GTK_CFLAGS)


# --------------------------------------------------------------------
endif
# --------------------------------------------------------------------


include $(root)/quickbuild.make
