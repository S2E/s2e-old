#===-- tools/static-translator/TypeExtractor/Makefile ------*- Makefile -*--===#
#
#
#
#===------------------------------------------------------------------------===#

LEVEL=../../..
TOOLNAME = typeextractor
USEDLIBS = executiontracer.a binaryreaders.a  utils.a
LINK_COMPONENTS = support system jit bitreader bitwriter ipo linker engine scalaropts ipa transformutils
#LLVMLIBS = clangBasic.a clangLex.a clangDriver.a clangFrontend.a clangParse.a \
#clangAST.a clangSema.a clangAnalysis.a clangCodeGen.a

LLVMLIBS=clangFrontend.a clangDriver.a clangCodeGen.a clangSema.a clangAnalysis.a clangRewrite.a clangAST.a clangParse.a clangLex.a clangBasic.a

include $(LEVEL)/Makefile.common

LIBS += $(SIGCPP_LIB) $(TOOL_LIBS)
CXXFLAGS += -fno-rtti
#-ltcmalloc
