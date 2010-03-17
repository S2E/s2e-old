LEVEL=../..

TOOLNAME=parser
BUILD_ARCHIVE=1

include $(LEVEL)/Makefile.common

LIBS+=-last -lsatsolver -lsimplifier -lconsteval -lconstantbv

CXXFLAGS+=-I$(ObjDir) -I$(PROJ_SRC_DIR) -I$(ObjDir)/../../AST/$(BuildMode) -DEXT_HASH_MAP
CompileCommonOpts:=$(filter-out -pedantic, $(CompileCommonOpts))

ObjectsO+=$(addprefix $(ObjDir)/, lexPL.o parsePL.o)
$(ToolBuildPath): $(addprefix $(ObjDir)/, lexPL.o parsePL.o)

LEX := flex
YACC := bison -d -y --debug -v

$(ObjDir)/main.o: $(ObjDir)/parsePL_defs.h

$(ObjDir)/lexPL.o: $(ObjDir)/lexPL.cpp
	$(Echo) "Compiling lexPL.cpp for $(BuildMode) build " $(PIC_FLAG)
	$(Verb) $(Compile.CXX) $< -o $@

$(ObjDir)/parsePL.o: $(ObjDir)/parsePL.cpp
	$(Echo) "Compiling parsePL.cpp for $(BuildMode) build " $(PIC_FLAG)
	$(Verb) $(Compile.CXX) $< -o $@

$(ObjDir)/lexPL.cpp: PL.lex $(ObjDir)/parsePL_defs.h ../AST/AST.h
	$(LEX) -o$(ObjDir)/lexPL.cpp $(PROJ_SRC_DIR)/PL.lex

$(ObjDir)/parsePL_defs.h: $(ObjDir)/y.tab.h
	@cp $(ObjDir)/y.tab.h $(ObjDir)/parsePL_defs.h

$(ObjDir)/parsePL.cpp: $(ObjDir)/y.tab.c
	@cp $(ObjDir)/y.tab.c $(ObjDir)/parsePL.cpp

$(ObjDir)/y.tab.c $(ObjDir)/y.tab.h: PL.y
	$(YACC) $(PROJ_SRC_DIR)/PL.y -o$(ObjDir)/y.tab.c

