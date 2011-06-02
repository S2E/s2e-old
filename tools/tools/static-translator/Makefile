#===-- tools/tools/Makefile --------------------------------*- Makefile -*--===#
#
#
#
#===------------------------------------------------------------------------===#

LEVEL=../..
PARALLEL_DIRS=TranslatorBitcode Runtime TypeExtractor
TOOLNAME = static-translator
USEDLIBS = executiontracer.a binaryreaders.a X86Translator.a  utils.a
LINK_COMPONENTS = support system jit bitreader bitwriter ipo linker engine scalaropts ipa transformutils
NO_PEDANTIC=1
SOURCES = StaticTranslator.cpp Passes/RevgenAlias.cpp \
          Passes/ConstantExtractor.cpp Passes/JumpTableExtractor.cpp \
          Passes/FunctionBuilder.cpp Passes/TargetBinary.cpp\
          Passes/CallBuilder.cpp Passes/InstructionInliner.cpp \
          Passes/CallingConvention.cpp

include $(LEVEL)/Makefile.common
NEWDIR := $(shell mkdir -p $(ObjDir)/$(S2E_SRC_ROOT)/target-i386)
NEWDIR := $(shell mkdir -p $(ObjDir)/$(S2E_SRC_ROOT)/tcg)
NEWDIR := $(shell mkdir -p $(ObjDir)/$(S2E_SRC_ROOT)/fpu)
NEWDIR := $(shell mkdir -p $(ObjDir)/Wrapper)
NEWDIR := $(shell mkdir -p $(ObjDir)/Passes)
NEWDIR := $(shell mkdir -p $(ObjDir)/CFG)


QEMUFLAGS=-I. -I$(S2E_SRC_ROOT) -I$(S2E_SRC_ROOT)/target-i386 \
        -I$(S2E_SRC_ROOT)/fpu -I$(S2E_SRC_ROOT)/tcg -I$(S2E_SRC_ROOT)/tcg/x86_64 -I..\
         -D_GNU_SOURCE -DNEED_CPU_H  -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE  -DSTATIC_TRANSLATOR\
          -Wredundant-decls -Wall -Wundef -Wendif-labels -Wwrite-strings -fno-strict-aliasing \
          -Wno-sign-compare -Wno-missing-field-initializers -fexceptions

ifeq ($(BuildMode),Debug)
QEMUFLAGS+=-g -O0
endif

CFLAGS+=$(QEMUFLAGS)
CXXFLAGS+=$(QEMUFLAGS)

LIBS += $(SIGCPP_LIB) $(TOOL_LIBS)

#-ltcmalloc
