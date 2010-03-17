 # STP (Simple Theorem Prover) top level makefile

LEVEL=..

USE_PARSER := 0

DIRS=AST sat simplifier bitvec c_interface constantbv stp

ifeq ($(USE_PARSER), 1)
DIRS += parser
endif

include Makefile.common

#$(LibName.A): $(addsufix /.dir, $(DIRS))
#
#ObjectsO:=$(addprefix -L$(ObjDir)/../, AST/$(BuildMode)/libast.a sat/$(BuildMode)/lubsatsolver.a simplifier/$(BuildMode)/libsimplifier.a bitvec/$(BuildMode)/libconsteval.a constantbv/$(BuildMode)/libconstantbv.a)

# NB: the TAGS target is a hack to get around this recursive make nonsense
# we want all the source and header files generated before we make tags
#.PHONY: all
#all: lib/libstp.a include/stp/c_interface.h
#
#AST/libast.a:
#	@$(MAKE) -q -C `dirname $@` || $(MAKE) -C `dirname $@`
#sat/libsatsolver.a: AST/libast.a
#	@$(MAKE) -q -C `dirname $@` || $(MAKE) -C `dirname $@`
#simplifier/libsimplifier.a: AST/libast.a
#	@$(MAKE) -q -C `dirname $@` || $(MAKE) -C `dirname $@`
#bitvec/libconsteval.a: AST/libast.a
#	@$(MAKE) -q -C `dirname $@` || $(MAKE) -C `dirname $@`
#constantbv/libconstantbv.a: AST/libast.a
#	@$(MAKE) -q -C `dirname $@` || $(MAKE) -C `dirname $@`
#c_interface/libcinterface.a: AST/libast.a
#	@$(MAKE) -q -C `dirname $@` || $(MAKE) -C `dirname $@`
#parser/parser: $(LIBS)
#	@$(MAKE) -q -C `dirname $@` || $(MAKE) -C `dirname $@`
#
#lib/libstp.a: $(LIBS)
#	@mkdir -p lib
#	rm -f $@
#	@for dir in $(DIRS); do \
#		$(AR) rc $@ $$dir/*.o; \
#	done
#	$(RANLIB) $@
#
#bin/stp: parser/parser $(LIBS)
#	@mkdir -p bin
#	@cp parser/parser $@
#
#include/stp/c_interface.h: $(LIBS)
#	@mkdir -p include/stp
#	@cp c_interface/c_interface.h $@
#
#.PHONY: clean
#clean:
#	rm -rf *~
#	rm -rf *.a
#	rm -rf lib/*.a
#	rm -rf bin/*~
#	rm -rf bin/stp
#	rm -rf *.log
#	rm -f TAGS
#	$(MAKE) clean -C AST
#	$(MAKE) clean -C sat
#	$(MAKE) clean -C simplifier
#	$(MAKE) clean -C bitvec
#	$(MAKE) clean -C parser
#	$(MAKE) clean -C c_interface
#	$(MAKE) clean -C constantbv
#
