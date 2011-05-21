#===-- tools/static-translator/TypeExtractor/Makefile ------*- Makefile -*--===#
#
#
#
#===------------------------------------------------------------------------===#

LEVEL=../../..
TOOLNAME = typeextractor
USEDLIBS = executiontracer.a binaryreaders.a  utils.a
LINK_COMPONENTS = support system jit bitreader bitwriter ipo linker engine scalaropts ipa transformutils
LLVMLIBS = clangBasic.a clangLex.a clangDriver.a clangFrontend.a clangParse.a \
clangAST.a clangSema.a clangAnalysis.a clangCodeGen.a

include $(LEVEL)/Makefile.common

LIBS += $(SIGCPP_LIB) $(TOOL_LIBS)
CXXFLAGS += -fno-rtti
#-ltcmalloc
