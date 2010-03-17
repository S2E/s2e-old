LEVEL=../..

LIBRARYNAME=consteval
BUILD_ARCHIVE=1
NO_INSTALL=1

include $(LEVEL)/Makefile.common

CXXFLAGS+=-I$(ObjDir)/../../AST/$(BuildMode) -DEXT_HASH_MAP
CompileCommonOpts:=$(filter-out -pedantic, $(CompileCommonOpts))

