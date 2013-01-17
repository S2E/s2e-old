S2ESRC:=$(CURDIR)/../s2e
S2EBUILD:=$(CURDIR)

OS := $(shell uname)
JOBS:=2

CP=cp
ifeq ($(OS),Darwin)
CP=gcp
JOBS := $(patsubst hw.ncpu:%,%,$(shell sysctl hw.ncpu))
else ifeq ($(OS),Linux)
JOBS := $(shell grep -c ^processor /proc/cpuinfo)
endif

all: all-release

all-release: stamps/qemu-make-release stamps/tools-make-release

all-debug: stamps/qemu-make-debug stamps/tools-make-debug

ifeq ($(shell ls qemu/vl.c 2>&1),qemu/vl.c)
    $(error You should not run make in S2E source directory!)
endif


LLVM_VERSION=3.0
LLVM_SRC=llvm-$(LLVM_VERSION).tar.gz
LLVM_SRC_DIR=llvm-$(LLVM_VERSION).src

CLANG_CC=$(shell which clang)
CLANG_CXX=$(shell which clang++)

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

$(LLVM_SRC):
	wget http://llvm.org/releases/$(LLVM_VERSION)/$(LLVM_SRC)

stamps/llvm-unpack: $(LLVM_SRC)
	tar -zxf $(LLVM_SRC)
	mkdir -p stamps && touch $@


########
# LLVM #
########

#Build it with the clang compiler.
stamps/llvm-configure: stamps/llvm-unpack
	mkdir -p llvm
	cd llvm && $(S2EBUILD)/$(LLVM_SRC_DIR)/configure \
		--prefix=$(S2EBUILD)/opt \
		--target=x86_64 --enable-targets=x86 --enable-jit \
		--enable-optimized --enable-assertions \
		CC=$(CLANG_CC) \
		CXX=$(CLANG_CXX)
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
	$(CP) -Rup $(S2ESRC)/stp stp
	mkdir -p stamps && touch $@

stamps/stp-configure: stamps/stp-copy
	cd stp && bash scripts/configure --with-prefix=$(S2EBUILD)/stp --with-fpic --with-g++=$(CLANG_CXX) --with-gcc=$(CLANG_CC)
	#cd stp && cp src/c_interface/c_interface.h include/stp
	mkdir -p stamps && touch $@

stamps/stp-make: stamps/stp-configure ALWAYS
	$(CP) -Rup $(S2ESRC)/stp stp
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
		--enable-exceptions --enable-assertions \
                CC=$(CLANG_CC) \
		CXX=$(CLANG_CXX)
	mkdir -p stamps && touch $@

stamps/klee-make-debug: stamps/klee-configure stamps/llvm-make-debug stamps/stp-make ALWAYS
	cd klee && make ENABLE_OPTIMIZED=0 CXXFLAGS="-g -O0" -j$(JOBS)
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
		--with-llvm=$(S2EBUILD)/llvm/Debug+Asserts  \
		--with-clang=$(CLANG_CC) \
		--with-stp=$(S2EBUILD)/stp \
		--with-klee=$(S2EBUILD)/klee/Debug+Asserts \
		--target-list=i386-s2e-softmmu,i386-softmmu \
		--enable-llvm \
		--enable-s2e \
		--enable-debug --compile-all-with-clang \
                $(EXTRA_QEMU_FLAGS)

	mkdir -p stamps && touch $@

stamps/qemu-configure-release: stamps/klee-configure klee/Release/bin/klee-config
	mkdir -p qemu-release
	cd qemu-release && $(S2ESRC)/qemu/configure \
		--prefix=$(S2EBUILD)/opt \
		--with-llvm=$(S2EBUILD)/llvm/Release+Asserts  \
		--with-clang=$(CLANG_CC) \
		--with-stp=$(S2EBUILD)/stp \
		--with-klee=$(S2EBUILD)/klee/Release+Asserts \
		--target-list=i386-s2e-softmmu,i386-softmmu \
		--enable-llvm \
		--enable-s2e --compile-all-with-clang \
                $(EXTRA_QEMU_FLAGS)

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
		--with-llvmsrc=$(S2EBUILD)/$(LLVM_SRC_DIR) \
		--with-llvmobj=$(S2EBUILD)/llvm \
		--with-s2esrc=$(S2ESRC)/qemu \
		--target=x86_64 --enable-assertions \
		CC=$(CLANG_CC) \
		CXX=$(CLANG_CXX)
	mkdir -p stamps && touch $@

stamps/tools-make-release: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1 -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/tools-make-debug: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=0 REQUIRES_RTTI=1 -j$(JOBS)
	mkdir -p stamps && touch $@

