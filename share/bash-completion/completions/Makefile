
topdir := ../../..


INSTALL_DIR = $(PREFIX)/share/bash-completion/completions

BUILD := quickstream

helper := $(topdir)/lib/quickstream/misc/quickstreamHelp


quickstream: quickstream.IN $(helper) $(topdir)/lib/builtInBlocks/listBuiltInBlocksPaths
	opts="$$($(helper) -O)" &&\
  opts_noarg="$$($(helper) -w)" &&\
  paths=$$(cat $(topdir)/lib/builtInBlocks/listBuiltInBlocksPaths) &&\
  sed $<\
    -e "s/@GEN_FILE@/\# This is a generated file from the quickstream package/g"\
    -e "s/@OPTS@/$$opts/g"\
    -e "s/@OPTS_NOARG@/$$opts_noarg/g"\
    -e "s!@builtInBlocks@!$$paths!g" > $@


include $(topdir)/quickbuild.make
