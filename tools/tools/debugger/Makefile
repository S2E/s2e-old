#===-- tools/klee/Makefile ---------------------------------*- Makefile -*--===#
#
#
#
#===------------------------------------------------------------------------===#

LEVEL=../..
TOOLNAME = debugger
USEDLIBS = executiontracer.a binaryreaders.a
LINK_COMPONENTS = support system

include $(LEVEL)/Makefile.common


LIBS += $(SIGCPP_LIB) -lbfd -lintl -liberty -lz
#-ltcmalloc
