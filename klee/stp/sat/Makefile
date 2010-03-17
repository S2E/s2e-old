LEVEL=../..

LIBRARYNAME=satsolver
BUILD_ARCHIVE=1
NO_INSTALL=1

SOURCES=Solver.C Simplifier.C

include $(LEVEL)/Makefile.common

CXXFLAGS+=-DEXT_HASH_MAP
CompileCommonOpts:=$(filter-out -pedantic, $(CompileCommonOpts))

ifndef DISABLE_AUTO_DEPENDENCIES

$(ObjDir)/%.o: %.C $(ObjDir)/.dir $(BUILT_SOURCES)
	$(Echo) "Compiling $*.C for $(BuildMode) build" $(PIC_FLAG)
	$(Verb) if $(Compile.CXX) $(DEPEND_OPTIONS) $< -o $(ObjDir)/$*.o ; \
	        $(DEPEND_MOVEFILE)

else

$(ObjDir)/%.o: %.C $(ObjDir)/.dir $(BUILT_SOURCES)
	$(Echo) "Compiling $*.C for $(BuildMode) build" $(PIC_FLAG)
	$(Compile.CXX) $< -o $@

endif

