
topdir := ../../..


INSTALL_DIR = $(PREFIX)/share/bash-completion/completions

BUILD := quickstream


builtIns := $(sort $(shell cat $(topdir)/lib/builtInBlocks.txt | grep -ve '^\#'))

#$(warning builtIns=$(builtIns))


helper := $(topdir)/lib/quickstream/misc/quickstreamHelp


quickstream: quickstream.IN $(helper) $(topdir)/lib/builtInBlocks.txt
	opts="$$($(helper) -O)" &&\
  opts_noarg="$$($(helper) -w)" &&\
  sed $<\
    -e "s/@GEN_FILE@/\# This is a generated file from the quickstream package/g"\
    -e "s/@OPTS@/$$opts/g"\
    -e "s/@OPTS_NOARG@/$$opts_noarg/g"\
    -e "s!@builtInBlocks@!$(builtIns)!g" > $@


include $(topdir)/quickbuild.make
