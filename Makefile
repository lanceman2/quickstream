# This file is for building and installing quickstream with quickbuild,
# not GNU autotools.  You can also build and install quickstream with GNU
# autotools.
#
# The make files with name 'makefile' are generated from 'makefile.am'
# using GNU autotools, specifically GNU automake; and we used the make
# file name 'Makefile' and sometimes 'GNUmakefile' for using quickbuild.

SUBDIRS :=\
 include/quickstream\
 lib\
 lib/quickstream/misc\
 bin\
 lib/quickstream/blocks/filters\
 lib/quickstream/blocks/controllers


ifneq ($(wildcard quickbuild.make),quickbuild.make)
$(error "First run './bootstrap'")
endif


ifeq ($(strip $(subst cleaner, clean, $(MAKECMDGOALS))),clean)
SUBDIRS +=\
 tests
endif


test:
	cd tests && $(MAKE) test


include quickbuild.make
