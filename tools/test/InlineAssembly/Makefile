#1. Compile test program to native, run it and save its output
#2. Compile test program to LLVM
#3. Run static translator on the LLVM program
#4. Run deinlined program, save its output
#5. Compare the outputs


LEVEL = ../..

SOURCES = asm1.diff asm2.diff asm3-multiple-output.diff asm4-spilled-regs.diff
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
	@echo $(REVGEN) -o $@ -asmdeinliner -logfile $*.log -bitcodelibrary=$(BITCODE_LIB) $< > $*.cmd
	@$(RUNSAFE) 200 0 /dev/null "$*.revgen" $(REVGEN) -o $@ -asmdeinliner -loglevel 0 -logfile $*.log -bitcodelibrary=$(BITCODE_LIB) -keeptmp $<

%.native.translated: %.bc.translated
	@$(LLC) -f -o=$@.s $<
	@$(CC) -m64 -o $@ $@.s	

%.diff: %.native %.native.translated
	@$(RUNSAFE) 10 0 /dev/null "$*.out-nat" "$(CURDIR)/$*.native" 12 23
	@$(RUNSAFE) 10 0 /dev/null "$*.out-translated" "$(CURDIR)/$*.native.translated" 12 23
	@$(CMPOUT) "$*.out-nat" "$*.out-translated" "$@"

	
test: $(SOURCES)
	rm tmp-*

.PRECIOUS: %.bc %.native.translated %.bc.translated %.native
