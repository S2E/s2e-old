
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

#Hack to avoid complaints from the C++ compiler
QEMU_CPPFLAGS := $(QEMU_CFLAGS:-Wold-style-definition=)
QEMU_CPPFLAGS := $(QEMU_CPPFLAGS:-Wold-style-declaration=)
QEMU_CPPFLAGS := $(QEMU_CPPFLAGS:-Wstrict-prototypes=)
QEMU_CPPFLAGS := $(QEMU_CPPFLAGS:-Wmissing-prototypes=)

S2E_TARGET_PATH := $(SRC_PATH)/target-i386
QEMU_CPPFLAGS +=  -I$(SRC_PATH)/fpu -I$(SRC_PATH)/tcg -I$(SRC_PATH)/tcg/i386  -I.. -I$(S2E_TARGET_PATH) -DNEED_CPU_H


%.o: %.cpp $(GENERATED_HEADERS)
	$(call quiet-command,$(CC) $(QEMU_CPPFLAGS) $(QEMU_DGFLAGS) $(CFLAGS) -c -o $@ $<,"  CPP   $(TARGET_DIR)$@")

%.o: %.c $(GENERATED_HEADERS)
	$(call quiet-command,$(CC) $(QEMU_CFLAGS) $(QEMU_DGFLAGS) $(CFLAGS) -c -o $@ $<,"  CC    $(TARGET_DIR)$@")

%.o: %.S
	$(call quiet-command,$(CC) $(QEMU_CFLAGS) $(QEMU_DGFLAGS) $(CFLAGS) -c -o $@ $<,"  AS    $(TARGET_DIR)$@")

%.o: %.m
	$(call quiet-command,$(CC) $(QEMU_CFLAGS) $(QEMU_DGFLAGS) $(CFLAGS) -c -o $@ $<,"  OBJC  $(TARGET_DIR)$@")

LINK = $(call quiet-command,$(CC) $(QEMU_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(1) $(ARLIBS_BEGIN) $(ARLIBS) $(ARLIBS_END) $(LIBS) -lstdc++,"  LINK  $(TARGET_DIR)$@")

%$(EXESUF): %.o
	$(call LINK,$^)

%.a:
	$(call quiet-command,rm -f $@ && $(AR) rcs $@ $^,"  AR    $(TARGET_DIR)$@")

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
