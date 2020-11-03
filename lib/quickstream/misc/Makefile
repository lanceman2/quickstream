
INSTALL_DIR = $(PREFIX)/lib/quickstream/misc

quickstreamHelp_SOURCES := quickstreamHelp.c

IN_VARS := PACKAGE_URL

BUILD_NO_INSTALL := qsOptions.h

qsOptions.h: quickstreamHelp
	./quickstreamHelp -c > $@

include ../../../quickbuild.make
