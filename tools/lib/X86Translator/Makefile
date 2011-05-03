#===-- lib/X86Translator/Makefile --------------------------*- Makefile -*--===#
#
#
#
#===------------------------------------------------------------------------===#

LEVEL=../..
LIBRARYNAME = X86Translator
USEDLIBS = executiontracer.a binaryreaders.a
NO_PEDANTIC=1

SOURCES = $(S2E_SRC_ROOT)/target-i386/translate.c \
          $(S2E_SRC_ROOT)/target-i386/helper.c \
          $(S2E_SRC_ROOT)/target-i386/op_helper.c \
          $(S2E_SRC_ROOT)/fpu/softfloat-native.c \
          $(S2E_SRC_ROOT)/tcg/tcg.c \
          $(S2E_SRC_ROOT)/tcg/tcg-llvm.cpp \
          $(S2E_SRC_ROOT)/cutils.c\
          $(S2E_SRC_ROOT)/translate-all.c\
          TranslatorWrapper.cpp  \
          Translator.cpp


include $(LEVEL)/Makefile.common

NEWDIR := $(shell mkdir -p $(ObjDir)/$(S2E_SRC_ROOT)/target-i386)
NEWDIR := $(shell mkdir -p $(ObjDir)/$(S2E_SRC_ROOT)/tcg)
NEWDIR := $(shell mkdir -p $(ObjDir)/$(S2E_SRC_ROOT)/fpu)


QEMUFLAGS=-I. -I$(S2E_SRC_ROOT) -I$(S2E_SRC_ROOT)/target-i386 \
        -I$(S2E_SRC_ROOT)/fpu -I$(S2E_SRC_ROOT)/tcg -I$(S2E_SRC_ROOT)/tcg/x86_64 -I..\
         -D_GNU_SOURCE -DNEED_CPU_H  -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE  -DSTATIC_TRANSLATOR\
          -Wredundant-decls -w -Wundef -Wendif-labels -Wwrite-strings -fno-strict-aliasing \
          -Wno-sign-compare -Wno-missing-field-initializers -fexceptions

CFLAGS+=$(QEMUFLAGS)
CXXFLAGS+=$(QEMUFLAGS)


#-ltcmalloc
