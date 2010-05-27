#===-- tools/klee/Makefile ---------------------------------*- Makefile -*--===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#

LEVEL=../..
TOOLNAME = pfprofiler
USEDLIBS = executiontracer.a binaryreaders.a
LINK_COMPONENTS = support system

include $(LEVEL)/Makefile.common


LIBS += -lsigc-2.0 -lbfd -lintl -liberty -lz
#-ltcmalloc
