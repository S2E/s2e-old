#===-- tools/tools/Makefile --------------------------------*- Makefile -*--===#
#
# The host must be 64-bit!
#
#===------------------------------------------------------------------------===#

LEVEL=../../..
LIBRARYNAME = X86BitcodeLibrary
MODULE_NAME = X86BitcodeLibrary
BYTECODE_LIBRARY = 1
NO_PEDANTIC=1
ENABLE_OPTIMIZED=1

SOURCES = $(S2E_SRC_ROOT)/target-i386/translate.c \
          $(S2E_SRC_ROOT)/target-i386/helper.c \
          $(S2E_SRC_ROOT)/target-i386/op_helper.c \
          $(S2E_SRC_ROOT)/fpu/softfloat-native.c

include $(LEVEL)/Makefile.common
NEWDIR := $(shell mkdir -p $(ObjDir)/$(S2E_SRC_ROOT)/target-i386)
NEWDIR := $(shell mkdir -p $(ObjDir)/$(S2E_SRC_ROOT)/fpu)

QEMUFLAGS=-m64 -I. -I$(S2E_SRC_ROOT) -I$(S2E_SRC_ROOT)/target-i386 \
        -I$(S2E_SRC_ROOT)/fpu -I$(S2E_SRC_ROOT)/tcg -I$(S2E_SRC_ROOT)/tcg/x86_64 -I..\
         -D_GNU_SOURCE -DNEED_CPU_H  -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE \
         -DS2E_LLVM_LIB -DSTATIC_TRANSLATOR\
         -Wredundant-decls -w -Wundef -Wendif-labels -Wwrite-strings -fno-strict-aliasing \
         -Wno-sign-compare -Wno-missing-field-initializers

CFLAGS+=$(QEMUFLAGS)
CXXFLAGS+=$(QEMUFLAGS)


#-ltcmalloc
