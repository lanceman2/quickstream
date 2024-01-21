# We make in all sub-directories in this directory, so long as there is a
# Makefile in them:
SUBDIRS := $(sort $(patsubst ./%/Makefile,%, $(dir $(wildcard ./*/Makefile))))


INSTALL_DIR = $(PREFIX)/lib/quickstream/blocks


include ../../../quickbuild.make
