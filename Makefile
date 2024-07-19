# This file is for building and installing quickstream with quickbuild,
# not GNU autotools.
#
# The make files with name 'makefile' are generated from 'makefile.am'
# using GNU autotools, specifically GNU automake; and we used the make
# file name 'Makefile' and sometimes 'GNUmakefile' for using quickbuild.

SUBDIRS :=\
 include\
 lib\
 lib/quickstream/blocks\
 lib/quickstream/misc\
 bin\
 share/doc/quickstream\
 share/bash-completion/completions


ifneq ($(wildcard quickbuild.make),quickbuild.make)
$(error "First run './bootstrap'")
endif
ifneq ($(wildcard config.make),config.make)
$(error "Now run './configure'")
endif



ifeq ($(strip $(subst cleaner, clean, $(MAKECMDGOALS))),clean)
SUBDIRS +=\
 tests
endif


test:
	cd tests && $(MAKE) test


include quickbuild.make
