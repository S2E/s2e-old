LEVEL = ../..

SOURCES = asm1.c
REVGEN=$(PROJ_OBJ_ROOT)/Debug/bin/static-translator
BITCODE_LIB=$(PROJ_OBJ_ROOT)/Release/lib/X86BitcodeLibrary.bc
RUNSAFE=$(PROJ_SRC_ROOT)/test/RunSafely.sh
CMPOUT=$(PROJ_SRC_ROOT)/test/CompareOutput.sh

include $(LEVEL)/Makefile.common


%.native: %.c
	@$(CC) -m64 -o $@ $<

%.bc: %.c
	@$(LLVMCC) -m64 -c -emit-llvm -o $@ $<	

%.bc.translated: %.bc
	@$(REVGEN) -o $@ -asmdeinliner -logfile $*.log -bitcodelibrary=$(BITCODE_LIB) $<

%.native.translated: %.bc.translated
	@$(LLC) -f -o=$@.s $<
	@$(CC) -m64 -o $@ $@.s	

%.diff: %.native %.native.translated
	@$(RUNSAFE) 10 0 /dev/null "$*.out-nat" "$(CURDIR)/$*.native" 12 23
	@$(RUNSAFE) 10 0 /dev/null "$*.out-translated" "$(CURDIR)/$*.native.translated" 12 23
	@$(CMPOUT) "$*.out-nat" "$*.out-translated" "$@"

	
test: asm1.diff
	rm tmp-*



#1. Compile test program to native, run it and save its output
#2. Compile test program to LLVM
#3. Run static translator on the LLVM program
#4. Run deinlined program, save its output
#5. Compare the outputs

# Try with multiple different inputs every program
