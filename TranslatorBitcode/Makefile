#===-- tools/tools/Makefile --------------------------------*- Makefile -*--===#
#
#
#
#===------------------------------------------------------------------------===#

LEVEL=../../..
LIBRARYNAME = TranslatorBitcode
MODULE_NAME = TranslatorBitCode
USEDLIBS = executiontracer.a binaryreaders.a
LINK_COMPONENTS = support system jit bitreader bitwriter ipo linker engine
BYTECODE_LIBRARY = 1
NO_PEDANTIC=1
SOURCES = $(S2E_SRC_ROOT)/target-i386/translate.c \
          $(S2E_SRC_ROOT)/target-i386/helper.c \
          $(S2E_SRC_ROOT)/target-i386/op_helper.c \
          $(S2E_SRC_ROOT)/fpu/softfloat-native.c

include $(LEVEL)/Makefile.common
NEWDIR := $(shell mkdir -p $(ObjDir)/$(S2E_SRC_ROOT)/target-i386)
NEWDIR := $(shell mkdir -p $(ObjDir)/$(S2E_SRC_ROOT)/fpu)


QEMUFLAGS=-I. -I$(S2E_SRC_ROOT) -I$(S2E_SRC_ROOT)/target-i386 \
        -I$(S2E_SRC_ROOT)/fpu -I$(S2E_SRC_ROOT)/tcg -I$(S2E_SRC_ROOT)/tcg/x86_64 -I..\
         -D_GNU_SOURCE -DNEED_CPU_H  -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE  -DSTATIC_TRANSLATOR -Wredundant-decls -Wall -Wundef -Wendif-labels -Wwrite-strings -fno-strict-aliasing \
         -DS2E_LLVM_LIB

CFLAGS+=$(QEMUFLAGS)
CXXFLAGS+=$(QEMUFLAGS)


#-ltcmalloc
