LEVEL = ../..

SOURCES = asm1.c

include $(LEVEL)/Makefile.common


%.native: %.c
	$(CC) -m64 -o $@ $<

%.bc: %.c
	$(LLVMCC) -m64 -c -emit-llvm -o $@ $<	

%.bc.translated: %.bc
	$(REVGEN) -o $@ -asmdeinliner -logfile $*.log -bitcodelibrary=$(BITCODE_LIB) $<

%.native.translated: %.bc.translated
	-$(LLC) -f -o=$@.s $<
	-$(CC) -m64 -o $@ $@.s	

%.diff: %.native %.native.translated
	echo testdiff

test: asm1.diff



REVGEN=$(PROJ_OBJ_ROOT)/Debug/bin/static-translator
BITCODE_LIB=$(PROJ_OBJ_ROOT)/Release/lib/X86BitcodeLibrary.bc

DIRS :=

#1. Compile test program to native, run it and save its output
#2. Compile test program to LLVM
#3. Run static translator on the LLVM program
#4. Run deinlined program, save its output
#5. Compare the outputs

# Try with multiple different inputs every program
