#===-- tools/tools/Makefile --------------------------------*- Makefile -*--===#
#
# THIS MUST BE COMPILED TO 64 BITS IN OPTIMIZED MODE!
#
#===------------------------------------------------------------------------===#

LEVEL=../../..
LIBRARYNAME = TranslatorRuntime
MODULE_NAME = TranslatorRuntime
USEDLIBS = 
LINK_COMPONENTS = support system jit bitreader bitwriter ipo linker engine
BYTECODE_LIBRARY = 1
NO_PEDANTIC=1
ENABLE_OPTIMIZED=1

include $(LEVEL)/Makefile.common

QEMUFLAGS=-m32 -I. -I$(S2E_SRC_ROOT) -I$(S2E_SRC_ROOT)/target-i386 \
        -I$(S2E_SRC_ROOT)/fpu -I$(S2E_SRC_ROOT)/tcg -I$(S2E_SRC_ROOT)/tcg/x86_64 -I..\
         -D_GNU_SOURCE -DNEED_CPU_H  -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE \
         -DS2E_LLVM_LIB \
         -Wredundant-decls -Wall -Wundef -Wendif-labels -Wwrite-strings -fno-strict-aliasing \
         -Wno-sign-compare -Wno-missing-field-initializers

CFLAGS+=$(QEMUFLAGS)
CXXFLAGS+=$(QEMUFLAGS)


#-ltcmalloc

