S2ESRC:=$(CURDIR)/../s2e
S2EBUILD:=$(CURDIR)

JOBS:=8

all: all-release

all-release: stamps/qemu-make-release stamps/tools-make-release

all-debug: stamps/qemu-make-debug stamps/tools-make-debug

ifeq ($(shell ls qemu/vl.c 2>&1),qemu/vl.c)
    $(error You should not run make in S2E source directory!)
endif


LLVM_VERSION=3.0
LLVM_SRC=llvm-$(LLVM_VERSION).tar.gz
LLVM_SRC_DIR=llvm-$(LLVM_VERSION).src
CLANG_SRC=clang-$(LLVM_VERSION).tar.gz
CLANG_SRC_DIR=clang-$(LLVM_VERSION).src

clean:
	rm -Rf tools qemu-release qemu-debug klee stp llvm
	rm -Rf llvm-$(LLVM_VERSION)
	rm -Rf $(LLVM_GCC_SRC)
	rm -Rf stamps

.PHONY: all all-debug all-release clean

ALWAYS:

#############
# Downloads #
#############

$(CLANG_SRC):
	wget http://llvm.org/releases/$(LLVM_VERSION)/$(CLANG_SRC)


$(LLVM_SRC):
	wget http://llvm.org/releases/$(LLVM_VERSION)/$(LLVM_SRC)

stamps/llvm-unpack: $(LLVM_SRC)
	tar -zxf $(LLVM_SRC)
	mkdir -p stamps && touch $@

stamps/clang-unpack: $(CLANG_SRC) stamps/llvm-unpack
	tar -zxf $(CLANG_SRC); \
	mv $(CLANG_SRC_DIR) $(LLVM_SRC_DIR)/tools/clang; \
	mkdir -p stamps && touch $@


########
# LLVM #
########

stamps/llvm-configure: stamps/clang-unpack stamps/llvm-unpack
	mkdir -p llvm
	cd llvm && $(S2EBUILD)/$(LLVM_SRC_DIR)/configure \
		--prefix=$(S2EBUILD)/opt \
		--target=x86_64 --enable-targets=x86 --enable-jit \
		--enable-optimized
	mkdir -p stamps && touch $@

stamps/llvm-make-debug: stamps/llvm-configure
	cd llvm && make ENABLE_OPTIMIZED=0 REQUIRES_RTTI=1 -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/llvm-make-release: stamps/llvm-configure
	cd llvm && make ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1 -j$(JOBS)
	mkdir -p stamps && touch $@

#######
# STP #
#######

stamps/stp-copy:
	cp -Rup $(S2ESRC)/stp stp
	mkdir -p stamps && touch $@

stamps/stp-configure: stamps/stp-copy
	cd stp && bash scripts/configure --with-prefix=$(S2EBUILD)/stp
	cd stp && cp src/c_interface/c_interface.h include/stp
	mkdir -p stamps && touch $@

stamps/stp-make: stamps/stp-configure ALWAYS
	cp -Rup $(S2ESRC)/stp stp
	cd stp && make -j$(JOBS)
	mkdir -p stamps && touch $@

stp/include/stp/c_interface.h: stamps/stp-configure

stp/lib/libstp.a: stamps/stp-make

########
# KLEE #
########

stamps/klee-configure: stamps/llvm-configure \
                       stp/include/stp/c_interface.h \
		       stp/lib/libstp.a
	mkdir -p klee
	cd klee && $(S2ESRC)/klee/configure \
		--prefix=$(S2EBUILD)/opt \
		--with-llvmsrc=$(S2EBUILD)/$(LLVM_SRC_DIR) \
		--with-llvmobj=$(S2EBUILD)/llvm \
		--with-stp=$(S2EBUILD)/stp \
		--target=x86_64 \
		--enable-exceptions
	mkdir -p stamps && touch $@

stamps/klee-make-debug: stamps/klee-configure stamps/llvm-make-debug stamps/stp-make ALWAYS
	cd klee && make ENABLE_OPTIMIZED=0 -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/klee-make-release: stamps/klee-configure stamps/llvm-make-release stamps/stp-make ALWAYS
	cd klee && make ENABLE_OPTIMIZED=1 -j$(JOBS)
	mkdir -p stamps && touch $@

########
# QEMU #
########

klee/Debug/bin/klee-config: stamps/klee-make-debug

klee/Release/bin/klee-config: stamps/klee-make-release

stamps/qemu-configure-debug: stamps/klee-configure klee/Debug/bin/klee-config
	mkdir -p qemu-debug
	cd qemu-debug && $(S2ESRC)/qemu/configure \
		--prefix=$(S2EBUILD)/opt \
		--with-llvm=$(S2EBUILD)/llvm/Debug  \
		--with-llvmgcc=$(S2EBUILD)/llvm/Debug/bin/clang \
		--with-stp=$(S2EBUILD)/stp \
		--with-klee=$(S2EBUILD)/klee/Debug \
		--target-list=i386-s2e-softmmu,i386-softmmu \
		--enable-llvm \
		--enable-s2e \
		--enable-debug
	mkdir -p stamps && touch $@

stamps/qemu-configure-release: stamps/klee-configure klee/Release/bin/klee-config
	mkdir -p qemu-release
	cd qemu-release && $(S2ESRC)/qemu/configure \
		--prefix=$(S2EBUILD)/opt \
		--with-llvm=$(S2EBUILD)/llvm/Release  \
		--with-clang=$(S2EBUILD)/llvm/Release/bin/clang \
		--with-stp=$(S2EBUILD)/stp \
		--with-klee=$(S2EBUILD)/klee/Release \
		--target-list=i386-s2e-softmmu,i386-softmmu \
		--enable-llvm \
		--enable-s2e
	mkdir -p stamps && touch $@

stamps/qemu-make-debug: stamps/qemu-configure-debug stamps/klee-make-debug ALWAYS
	cd qemu-debug && make -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/qemu-make-release: stamps/qemu-configure-release stamps/klee-make-release ALWAYS
	cd qemu-release && make -j$(JOBS)
	mkdir -p stamps && touch $@

#########
# Tools #
#########

stamps/tools-configure: stamps/llvm-configure
	mkdir -p tools
	cd tools && $(S2ESRC)/tools/configure \
		--with-llvmsrc=$(S2EBUILD)/llvm-$(LLVM_VERSION) \
		--with-llvmobj=$(S2EBUILD)/llvm \
		--with-s2esrc=$(S2ESRC)/qemu \
		--target=x86_64
	mkdir -p stamps && touch $@

stamps/tools-make-release: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=1 -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/tools-make-debug: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=0 -j$(JOBS)
	mkdir -p stamps && touch $@

