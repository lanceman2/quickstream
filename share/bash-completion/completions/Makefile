
INSTALL_DIR = $(PREFIX)/share/bash-completion/completions

BUILD := quickstream

helper := ../../../lib/quickstream/misc/quickstreamHelp

quickstream: quickstream.IN
	opts="$$($(helper) -O)" && opts_noarg="$$($(helper) -w)" &&\
	     sed $^\
 		-e "s/@GEN_FILE@/\# This is a generated file/g"\
 		-e "s/@OPTS@/$$opts/g"\
 		-e "s/@OPTS_NOARG@/$$opts_noarg/g" > $@


include ../../../quickbuild.make
