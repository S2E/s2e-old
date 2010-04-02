##===- tools/klee-config/Makefile --------------------------*- Makefile -*-===##
#
#                     The KLEE Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##

LEVEL = ../..

EXTRA_DIST = LibDeps.txt FinalLibDeps.txt klee-config.in.in find-cycles.pl
REQUIRES_EH := 1

include $(LEVEL)/Makefile.common

# If we don't have Perl, we can't generate the library dependencies upon which 
# klee-config depends. Therefore, only if we detect perl will we do anything
# useful.
ifeq ($(HAVE_PERL),1)

# Combine preprocessor flags (except for -I) and CXX flags.
SUB_CPPFLAGS = ${CPP.BaseFlags}
SUB_CFLAGS   = ${CPP.BaseFlags} ${C.Flags}
SUB_CXXFLAGS = ${CPP.BaseFlags} ${CXX.Flags}

# This is blank for now.  We need to be careful about adding stuff here:
# LDFLAGS tend not to be portable, and we don't currently require the
# user to use libtool when linking against KLEE.
SUB_LDFLAGS = 

FinalLibDeps = $(PROJ_OBJ_DIR)/FinalLibDeps.txt

# XXX
ifeq (0,1)
LibDeps      = $(PROJ_OBJ_DIR)/LibDeps.txt
LibDepsTemp  = $(PROJ_OBJ_DIR)/LibDeps.txt.tmp
GenLibDeps   = $(PROJ_SRC_ROOT)/utils/GenLibDeps.pl

$(LibDepsTemp): $(GenLibDeps) $(LibDir) $(wildcard $(LibDir)/*.a $(LibDir)/*.o)
	$(Echo) "Regenerating LibDeps.txt.tmp"
	$(Verb) $(PERL) $(GenLibDeps) -flat $(LibDir) "$(NM_PATH)" > $(LibDepsTemp)

$(LibDeps): $(LibDepsTemp)
	$(Verb) $(CMP) -s $@ $< || ( $(CP) $< $@ && \
	  $(EchoCmd) Updated LibDeps.txt because dependencies changed )

# Find all the cyclic dependencies between various KLEE libraries, so we
# don't have to process them at runtime.
$(FinalLibDeps): find-cycles.pl $(LibDeps)
	$(Echo) "Checking for cyclic dependencies between KLEE libraries."
	$(Verb) $(PERL) $< < $(LibDeps) > $@ || rm -f $@

else
FinalLibDeps      = $(PROJ_SRC_DIR)/FinalLibDeps.txt
endif

# Rerun our configure substitutions as needed.
ConfigInIn = $(PROJ_SRC_DIR)/klee-config.in.in
klee-config.in: $(ConfigInIn) $(ConfigStatusScript)
	$(Verb) cd $(PROJ_OBJ_ROOT) ; \
		$(ConfigStatusScript) tools/klee-config/klee-config.in

# Build our final script.
$(ToolDir)/klee-config: klee-config.in $(FinalLibDeps)
	$(Echo) "Building klee-config script."
	$(Verb) mkdir -p $(ToolDir)
	$(Verb) $(ECHO) 's/@KLEE_CPPFLAGS@/$(subst /,\/,$(SUB_CPPFLAGS))/' \
	  > temp.sed
	$(Verb) $(ECHO) 's/@KLEE_CFLAGS@/$(subst /,\/,$(SUB_CFLAGS))/' \
	  >> temp.sed
	$(Verb) $(ECHO) 's/@KLEE_CXXFLAGS@/$(subst /,\/,$(SUB_CXXFLAGS))/' \
	  >> temp.sed
	$(Verb) $(ECHO) 's/@KLEE_LDFLAGS@/$(subst /,\/,$(SUB_LDFLAGS))/' \
	  >> temp.sed
	$(Verb) $(ECHO) 's/@KLEE_BUILDMODE@/$(subst /,\/,$(BuildMode))/' \
	  >> temp.sed
	$(Verb) $(SED) -f temp.sed < $< > $@
	$(Verb) $(RM) temp.sed
	$(Verb) cat $(FinalLibDeps) >> $@
	$(Verb) chmod +x $@

else
# We don't have perl, just generate a dummy klee-config
$(ToolDir)/klee-config:
	$(Echo) "Building place holder klee-config script."
	$(Verb) $(ECHO) 'echo klee-config: Perl not found so klee-config could not be generated' >> $@
	$(Verb) chmod +x $@

endif
# Hook into the standard Makefile rules.
all-local:: $(ToolDir)/klee-config
# XXX
ifeq (0,1)
clean-local::
	$(Verb) $(RM) -f $(ToolDir)/klee-config klee-config.in $(FinalLibDeps) \
	  $(LibDeps) GenLibDeps.out
else
clean-local::
	$(Verb) $(RM) -f $(ToolDir)/klee-config klee-config.in \
	  $(LibDeps) GenLibDeps.out
endif
install-local:: all-local
	$(Echo) Installing klee-config
	$(Verb) $(MKDIR) $(PROJ_bindir)
	$(Verb) $(ScriptInstall) $(ToolDir)/klee-config $(PROJ_bindir)

