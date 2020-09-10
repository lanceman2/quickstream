# This file is for building and installing quickstream with quickbuild,
# not GNU autotools.  You can build and install quickstream with GNU
# autotools.
#
# The make files with name 'makefile' are generated from 'makefile.am'
# using GNU autotools, specifically GNU automake; and we used the make
# file name 'Makefile' and sometimes 'GNUmakefile' for using quickbuild.

SUBDIRS :=\
 lib\
 bin


ifneq ($(wildcard quickbuild.make),quickbuild.make)
$(error "First run './bootstrap'")
endif


include quickbuild.make
