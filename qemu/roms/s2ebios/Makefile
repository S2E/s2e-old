all: build-all

include ../../config-host.mak
include $(SRC_PATH)/rules.mak

VPATH=$(SRC_PATH)/roms/s2ebios
S2E_BIOS_SOURCE=$(VPATH)/s2e-inst.asm $(VPATH)/s2e-bios-low.asm $(VPATH)/s2e-bios-up.asm $(VPATH)/s2e-test.asm $(VPATH)/init.asm  $(VPATH)/s2e-ndis-test.asm

.PHONY : all clean build-all

build-all: s2e-bios.bin

%.asm: 
	

s2e-bios-low.bin : $(S2E_BIOS_SOURCE)
	$(call quiet-command,$(ASM) -O1 -i$(VPATH)/ -f bin -o $@ $(VPATH)/s2e-bios-low.asm,"  Building $(TARGET_DIR)$@")

s2e-bios-up.bin : $(S2E_BIOS_SOURCE)
	$(call quiet-command,$(ASM) -O1 -i$(VPATH)/ -f bin -o $@ $(VPATH)/s2e-bios-up.asm,"  Building $(TARGET_DIR)$@")

s2e-bios.bin: s2e-bios-up.bin s2e-bios-low.bin
	dd if=s2e-bios-low.bin of=$@ bs=65536 count=1
	dd if=s2e-bios-up.bin of=$@ bs=65536 count=1 seek=1

clean:
	rm -f *.o *.d *.raw *.img *.bin *~
