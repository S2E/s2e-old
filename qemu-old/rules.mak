
# Don't use implicit rules or variables
# we have explicit rules for everything
MAKEFLAGS += -rR

# Files with this suffixes are final, don't try to generate them
# using implicit rules
%.d:
%.h:
%.c:
%.cpp:
%.m:
%.mak:

# Flags for dependency generation
QEMU_DGFLAGS += -MMD -MP -MT $@

ifdef CONFIG_ASAN

%.o: %.c $(GENERATED_HEADERS)
	$(call quiet-command,$(ASANCC) $(QEMU_CFLAGS) $(QEMU_CCFLAGS) $(QEMU_DGFLAGS) $(CFLAGS) $(ASAN_FLAGS) -c -o $@ $<,"  ASANCC    $(TARGET_DIR)$@")

%.o: %.cpp $(GENERATED_HEADERS)
	$(call quiet-command,$(ASANCXX) $(QEMU_CFLAGS) $(QEMU_CXXFLAGS) $(QEMU_DGFLAGS) $(CXXFLAGS) $(ASAN_FLAGS) -c -o $@ $<,"  ASANCXX   $(TARGET_DIR)$@")

else

%.o: %.c $(GENERATED_HEADERS)
	$(call quiet-command,$(LLVMCC) $(QEMU_CFLAGS) $(QEMU_CCFLAGS) $(QEMU_DGFLAGS) $(CFLAGS) -c -o $@ $<,"  CC    $(TARGET_DIR)$@")

%.o: %.cpp $(GENERATED_HEADERS)
	$(call quiet-command,$(LLVMCC) $(QEMU_CFLAGS) $(QEMU_CXXFLAGS) $(QEMU_DGFLAGS) $(CXXFLAGS) -c -o $@ $<,"  CXX   $(TARGET_DIR)$@")

endif

%.o: %.S
	$(call quiet-command,$(CC) $(QEMU_CFLAGS) $(QEMU_CCFLAGS) $(QEMU_DGFLAGS) $(CFLAGS) -c -o $@ $<,"  AS    $(TARGET_DIR)$@")

%.o: %.m
	$(call quiet-command,$(CC) $(QEMU_CFLAGS) $(QEMU_CCFLAGS) $(QEMU_DGFLAGS) $(CFLAGS) -O2 -c -o $@ $<,"  OBJC  $(TARGET_DIR)$@")

%.o: %.asm
	$(call quiet-command,$(ASM) $(QEMU_ASMFLAGS) -o $@ $<,"  ASM  $(TARGET_DIR)$@")


LINK = $(call quiet-command,$(LINKER) $(QEMU_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(1) $(ARLIBS_BEGIN) $(ARLIBS) $(ARLIBS_END) $(LIBS) $(ASAN_LIBPATH) $(ASAN_LDFLAGS),"  LINK  $(TARGET_DIR)$@")

%$(EXESUF): %.o
	$(call LINK,$^)

%.a:
	$(call quiet-command,rm -f $@ && $(AR) rcs $@ $^,"  AR    $(TARGET_DIR)$@")

%.bc: %.c $(GENERATED_HEADERS)
	$(call quiet-command,$(LLVMCC) $(filter-out -g -Wold-style-declaration, $(QEMU_CFLAGS) $(QEMU_CCFLAGS) $(CFLAGS)) -c -DS2E_LLVM_LIB -emit-llvm -o $@ $<,"  LLVMCC    $(TARGET_DIR)$@")

%.bca:
	$(call quiet-command,rm -f $@ && $(LLVMAR) rcs $@ $^,"  LLVMAR    $(TARGET_DIR)$@")

quiet-command = $(if $(V),$1,$(if $(2),@echo $2 && $1, @$1))

# cc-option
# Usage: CFLAGS+=$(call cc-option, -falign-functions=0, -malign-functions=0)

cc-option = $(if $(shell $(CC) $1 $2 -S -o /dev/null -xc /dev/null \
              >/dev/null 2>&1 && echo OK), $2, $3)

# Generate timestamp files for .h include files

%.h: %.h-timestamp
	@test -f $@ || cp $< $@

%.h-timestamp: %.mak
	$(call quiet-command, sh $(SRC_PATH)/create_config < $< > $@, "  GEN   $*.h")
	@cmp $@ $*.h >/dev/null 2>&1 || cp $@ $*.h

# will delete the target of a rule if commands exit with a nonzero exit status
.DELETE_ON_ERROR:
