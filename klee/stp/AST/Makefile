LEVEL=../..

LIBRARYNAME=ast
BUILD_ARCHIVE=1
NO_INSTALL=1

SOURCES=AST.cpp ASTUtil.cpp BitBlast.cpp SimpBool.cpp ToCNF.cpp ToSAT.cpp Transform.cpp

include $(LEVEL)/Makefile.common

ObjectsO+=$(ObjDir)/ASTKind.o
$(LibName.A): $(ObjDir)/ASTKind.o
$(addprefix $(ObjDir)/, $(SOURCES:.cpp=.o)): $(ObjDir)/ASTKind.h

CXXFLAGS+=-I$(ObjDir) -DEXT_HASH_MAP
CompileCommonOpts:=$(filter-out -pedantic, $(CompileCommonOpts))

# ASTKind.h and ASTKind.cpp are automatically generated
$(ObjDir)/ASTKind.h $(ObjDir)/ASTKind.cpp: ASTKind.kinds genkinds.pl
	$(Echo) "Generating ASTKind.h and ASTKind.cpp for $(BuildMode) build " $(PIC_FLAG)
	$(Verb) mkdir -p $(ObjDir)
	$(Verb) ln -sf $(PROJ_SRC_DIR)/ASTKind.kinds $(ObjDir)/ASTKind.kinds
	$(Verb) cd $(ObjDir) && $(PROJ_SRC_DIR)/genkinds.pl

$(ObjDir)/ASTKind.o: $(ObjDir)/ASTKind.cpp $(ObjDir)/ASTKind.h
	$(Echo) "Compiling ASTKind.cpp for $(BuildMode) build " $(PIC_FLAG)
	$(Verb) $(Compile.CXX) $< -o $@

