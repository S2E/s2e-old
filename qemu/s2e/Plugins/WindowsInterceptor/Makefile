include ../config.mak

PLUGIN=osplg_windows.dll
SOURCE_DIRS=.

LLVM_GCC=llvm-gcc
LLVM_DIS=llvm-dis
CC=gcc
CPP=g++
LLVM_LD=llvm-ld

CPPFLAGS=-m64 -Wall -g   -D__SIMULATOR -D__STDC_LIMIT_MACROS -I. -I$(S2E_SRC) -I$(S2E_SRC)/target-i386 -I$(S2E_OBJ)/ -I$(S2E_OBJ)/i386-s2e-softmmu
CPPFLAGS+=-I$(S2E_SRC) -I$(S2E_SRC)/fpu -I$(S2E_SRC)/tcg -I$(S2E_SRC)/target-i386 -DNEED_CPU_H
CPPFLAGS+=$(foreach dir, $(LLVM_INC_DIR), -I$(dir))
LDFLAGS+=-m64 $(foreach dir, $(LLVM_LIB_DIR), -L$(dir)) -shared -Wl,--output-def -Wl,"$(BUILD_PREFIX)/exp.def"

ifdef CONFIG_WIN32
LIBS+=-lpsapi -limagehlp
else
LIBS+=-lpthread -ldl

endif


#######################################################################
BUILD_PREFIX=$(BUILD_DIR)/$(BUILD_MODE)
SOURCE_FILES=$(foreach dir, $(SOURCE_DIRS), $(wildcard $(dir)/*.cpp))
TMPOBJECT_FILES=$(SOURCE_FILES:.cpp=.o)
OBJECT_FILES=$(addprefix $(BUILD_PREFIX)/, $(TMPOBJECT_FILES))


$(BUILD_PREFIX)/%.o: %.cpp
	@echo [$(CXX)] $<
	$(CXX) $(CFLAGS) $(CPPFLAGS) $(BASE_CFLAGS) -c -o $@ $<
	

$(BUILD_PREFIX)/$(PLUGIN): $(OBJECT_FILES)
	@echo [LINK] $(PLUGIN)
	@$(CPP) -pg  -o $@ $^  $(LDFLAGS) $(LLVM_LIBS) $(LIBS)

all: $(BUILD_PREFIX)/$(PLUGIN)

clean:
	rm -rf $(OBJECT_FILES) $(BUILD_PREFIX)/$(PLUGIN)

install: $(BUILD_PREFIX)/$(PLUGIN)
	cp $(BUILD_PREFIX)/$(PLUGIN) $(PREFIX)
