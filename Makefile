#Environment variables:
#
#  BUILD_ARCH=corei7, etc...
#      Overrides the default clang -march settings.
#      Useful to build S2E in VirtualBox or in other VMs that do not support
#      some advanced instruction sets.
#      Used by STP only for now.
#
#  EXTRA_QEMU_FLAGS=...
#      Pass additional flags to QEMU's configure script.
#
#

S2ESRC:=$(CURDIR)/../s2e
S2EBUILD:=$(CURDIR)

OS := $(shell uname)
JOBS:=2

ifeq ($(OS),Darwin)
JOBS := $(patsubst hw.ncpu:%,%,$(shell sysctl hw.ncpu))
else ifeq ($(OS),Linux)
JOBS := $(shell grep -c ^processor /proc/cpuinfo)
endif

all: all-release

all-release: stamps/qemu-make-release stamps/tools-make-release

all-debug: stamps/qemu-make-debug stamps/tools-make-debug

ifeq ($(wildcard qemu/vl.c),qemu/vl.c)
    $(error You should not run make in the S2E source directory!)
endif


LLVM_VERSION=3.2
LLVM_SRC=llvm-$(LLVM_VERSION).src.tar.gz
LLVM_SRC_DIR=llvm-$(LLVM_VERSION).src
LLVM_NATIVE_SRC_DIR=llvm-$(LLVM_VERSION).src.native
CLANG_SRC=clang-$(LLVM_VERSION).src.tar.gz
CLANG_SRC_DIR=clang-$(LLVM_VERSION).src
COMPILER_RT_SRC=compiler-rt-$(LLVM_VERSION).src.tar.gz
COMPILER_RT_SRC_DIR=compiler-rt-$(LLVM_VERSION).src

clean:
	rm -Rf tools qemu-release qemu-debug klee stp llvm llvm-native
	rm -Rf llvm-$(LLVM_VERSION) $(LLVM_SRC_DIR) $(CLANG_SRC_DIR)
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

$(COMPILER_RT_SRC):
	wget http://llvm.org/releases/$(LLVM_VERSION)/$(COMPILER_RT_SRC)

stamps/llvm-unpack: $(LLVM_SRC)
	tar -zxf $(LLVM_SRC)
	cp -r $(LLVM_SRC_DIR) $(LLVM_NATIVE_SRC_DIR)
	mkdir -p stamps && touch $@

stamps/clang-unpack: $(CLANG_SRC) stamps/llvm-unpack
	tar -zxf $(CLANG_SRC)
	mv $(CLANG_SRC_DIR) $(LLVM_NATIVE_SRC_DIR)/tools/clang
	mkdir -p stamps && touch $@

stamps/compiler-rt-unpack: $(COMPILER_RT_SRC) stamps/llvm-unpack
	tar -zxf $(COMPILER_RT_SRC)
	mv $(COMPILER_RT_SRC_DIR) $(LLVM_NATIVE_SRC_DIR)/projects/compiler-rt
	mkdir -p stamps && touch $@



########
# LLVM #
########

CLANG_CC=$(S2EBUILD)/llvm-native/Release/bin/clang
CLANG_CXX=$(S2EBUILD)/llvm-native/Release/bin/clang++

#First build it with the system's compiler
stamps/llvm-configure-native: stamps/clang-unpack stamps/llvm-unpack stamps/compiler-rt-unpack
	mkdir -p llvm-native
	cd llvm-native && $(S2EBUILD)/$(LLVM_NATIVE_SRC_DIR)/configure \
		--prefix=$(S2EBUILD)/opt \
		--enable-jit --enable-optimized --disable-assertions #compiler-rt won't build if we specify explicit targets...
	mkdir -p stamps && touch $@

stamps/llvm-make-release-native: stamps/llvm-configure-native
	cd llvm-native && make ENABLE_OPTIMIZED=1 -j$(JOBS)
	mkdir -p stamps && touch $@


#Then, build LLVM with the clang compiler.
#Note that we build LLVM without clang and compiler-rt, because S2E does not need them.
stamps/llvm-configure: stamps/llvm-make-release-native
	mkdir -p llvm
	cd llvm && $(S2EBUILD)/$(LLVM_SRC_DIR)/configure \
	--prefix=$(S2EBUILD)/opt \
	--target=x86_64 --enable-targets=x86 --enable-jit \
	--enable-optimized \
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
	cp -Rp $(S2ESRC)/stp stp
	mkdir -p stamps && touch $@

stamps/stp-configure: stamps/stp-copy
	cd stp && scripts/configure --with-prefix=$(S2EBUILD)/stp --with-fpic --with-g++=$(CLANG_CXX) --with-gcc=$(CLANG_CC) --with-cryptominisat2
	mkdir -p stamps && touch $@

stamps/stp-make: stamps/stp-configure ALWAYS
	cd stp && make 
	mkdir -p stamps && touch $@

stp/include/stp/c_interface.h: stamps/stp-configure

stp/lib/libstp.a: stamps/stp-make


#ASAN-enabled STP
#XXX: need to fix the STP build to actually use ASAN...

stamps/stp-copy-asan:
	cp -Rp $(S2ESRC)/stp stp-asan
	mkdir -p stamps && touch $@

stamps/stp-configure-asan: stamps/stp-copy-asan
	cd stp-asan && scripts/configure --with-prefix=$(S2EBUILD)/stp-asan --with-fpic --with-g++=$(CLANG_CXX) --with-gcc=$(CLANG_CC) --with-address-sanitizer
	mkdir -p stamps && touch $@

stamps/stp-make-asan: stamps/stp-configure-asan ALWAYS
	cd stp-asan && make -j$(JOBS)
	mkdir -p stamps && touch $@


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
		--enable-exceptions \
		CC=$(CLANG_CC) \
		CXX=$(CLANG_CXX)
	mkdir -p stamps && touch $@

stamps/klee-make-debug: stamps/klee-configure stamps/llvm-make-debug stamps/stp-make ALWAYS
	cd klee && make ENABLE_OPTIMIZED=0 CXXFLAGS="-g -O0" -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/klee-make-release: stamps/klee-configure stamps/llvm-make-release stamps/stp-make ALWAYS
	cd klee && make ENABLE_OPTIMIZED=1 -j$(JOBS)
	mkdir -p stamps && touch $@


#ASAN-enabled KLEE
ASAN_CXX_LD_FLAGS = CXXFLAGS="-O0 -g -fsanitize=address" LDFLAGS="-g -fsanitize=address"

stamps/klee-configure-asan: stamps/llvm-make-release stamps/stp-make-asan
	mkdir -p klee-asan
	cd klee-asan && $(S2ESRC)/klee/configure \
		--prefix=$(S2EBUILD)/opt \
		--with-llvmsrc=$(S2EBUILD)/$(LLVM_SRC_DIR) \
		--with-llvmobj=$(S2EBUILD)/llvm \
		--with-stp=$(S2EBUILD)/stp-asan \
		--target=x86_64 \
		--enable-exceptions \
	    CC=$(CLANG_CC) \
		CXX=$(CLANG_CXX)\
		$(ASAN_CXX_LD_FLAGS)
	mkdir -p stamps && touch $@

stamps/klee-make-release-asan: stamps/klee-configure-asan stamps/stp-make-asan
	cd klee-asan && make ENABLE_OPTIMIZED=1 $(ASAN_CXX_LD_FLAGS) -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/klee-make-debug-asan: stamps/llvm-make-debug stamps/klee-configure-asan stamps/stp-make-asan
	cd klee-asan && make ENABLE_OPTIMIZED=0 $(ASAN_CXX_LD_FLAGS) -j$(JOBS)
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
		--with-llvm=$(S2EBUILD)/llvm/Debug+Asserts \
		--cc=$(CLANG_CC) \
		--cxx=$(CLANG_CXX) \
		--with-stp=$(S2EBUILD)/stp \
		--with-klee=$(S2EBUILD)/klee/Debug+Asserts \
		--target-list=i386-s2e-softmmu,i386-softmmu \
		--enable-llvm \
		--enable-s2e \
		--enable-debug \
		--with-pkgversion=S2E \
		$(EXTRA_QEMU_FLAGS)

	mkdir -p stamps && touch $@

stamps/qemu-configure-release: stamps/klee-configure klee/Release/bin/klee-config
	mkdir -p qemu-release
	cd qemu-release && $(S2ESRC)/qemu/configure \
		--prefix=$(S2EBUILD)/opt \
		--with-llvm=$(S2EBUILD)/llvm/Release+Asserts \
		--cc=$(CLANG_CC) \
		--cxx=$(CLANG_CXX) \
		--with-stp=$(S2EBUILD)/stp \
		--with-klee=$(S2EBUILD)/klee/Release+Asserts \
		--target-list=i386-s2e-softmmu,i386-softmmu \
		--enable-llvm \
		--enable-s2e\
		--with-pkgversion=S2E \
		$(EXTRA_QEMU_FLAGS)

	mkdir -p stamps && touch $@

stamps/qemu-make-debug: stamps/qemu-configure-debug stamps/klee-make-debug
	cd qemu-debug && make -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/qemu-make-release: stamps/qemu-configure-release stamps/klee-make-release
	cd qemu-release && make -j$(JOBS)
	mkdir -p stamps && touch $@

#ASAN-enabled QEMU
stamps/qemu-configure-release-asan: stamps/klee-make-release-asan
	mkdir -p qemu-release-asan
	cd qemu-release-asan && $(S2ESRC)/qemu/configure \
		--prefix=$(S2EBUILD)/opt \
		--with-llvm=$(S2EBUILD)/llvm/Release+Asserts \
		--cc=$(CLANG_CC) \
		--cxx=$(CLANG_CXX) \
		--with-stp=$(S2EBUILD)/stp-asan \
		--with-klee=$(S2EBUILD)/klee-asan/Release+Asserts \
		--target-list=i386-s2e-softmmu,i386-softmmu \
		--enable-llvm \
		--enable-s2e --enable-address-sanitizer \
		--with-pkgversion=S2E \
		$(EXTRA_QEMU_FLAGS)

	mkdir -p stamps && touch $@

stamps/qemu-configure-debug-asan: stamps/klee-make-debug-asan
	mkdir -p qemu-debug-asan
	cd qemu-debug-asan && $(S2ESRC)/qemu/configure \
		--prefix=$(S2EBUILD)/opt \
		--with-llvm=$(S2EBUILD)/llvm/Debug+Asserts \
		--cc=$(CLANG_CC) \
		--cxx=$(CLANG_CXX) \
		--with-stp=$(S2EBUILD)/stp-asan \
		--with-klee=$(S2EBUILD)/klee-asan/Debug+Asserts \
		--target-list=i386-s2e-softmmu,i386-softmmu \
		--enable-llvm \
		--enable-s2e --enable-address-sanitizer \
		--enable-debug \
		--with-pkgversion=S2E \
		$(EXTRA_QEMU_FLAGS)
	mkdir -p stamps && touch $@

stamps/qemu-make-debug-asan: stamps/qemu-configure-debug-asan stamps/klee-make-debug-asan
	cd qemu-debug-asan && make -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/qemu-make-release-asan: stamps/qemu-configure-release-asan stamps/klee-make-release-asan
	cd qemu-release-asan && make -j$(JOBS)
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
		--target=x86_64 \
		CC=$(CLANG_CC) \
		CXX=$(CLANG_CXX)
	mkdir -p stamps && touch $@

stamps/tools-make-release: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1 -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/tools-make-debug: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=0 REQUIRES_RTTI=1 -j$(JOBS)
	mkdir -p stamps && touch $@


############
#Guest tools
############
stamps/guest-tools: ALWAYS
	mkdir -p guest-tools && cd $(S2ESRC)/guest && make clean && make install BUILD_DIR=$(S2EBUILD)/guest-tools
	mkdir -p stamps && touch $@
