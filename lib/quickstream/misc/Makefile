
topdir := ../../..

SUBDIRS := test_blocks

INSTALL_DIR = $(PREFIX)/lib/quickstream/misc

quickstreamHelp_SOURCES := quickstreamHelp.c


IN_VARS := PACKAGE_URL

BUILD_NO_INSTALL := qsOptions.h

qsOptions.h: quickstreamHelp
	if ! ./$< -c > $@ ; then\
		rm -f $@; exit 1; fi


include $(topdir)/quickbuild.make
