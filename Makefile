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
CLANG_DEST_DIR=$(LLVM_SRC_DIR)/tools/clang
COMPILER_RT_SRC=compiler-rt-$(LLVM_VERSION).src.tar.gz
COMPILER_RT_SRC_DIR=compiler-rt-$(LLVM_VERSION).src
COMPILER_RT_DEST_DIR=$(LLVM_NATIVE_SRC_DIR)/projects/compiler-rt

clean:
	rm -Rf klee klee-asan qemu-debug qemu-debug-asan qemu-release qemu-release-asan
	rm -Rf stamps

guestclean:
	make -C $(S2ESRC)/guest clean

distclean: clean guestclean
	rm -Rf guest-tools llvm llvm-native stp stp-asan tools
	rm -Rf $(COMPILER_RT_SRC_DIR) $(LLVM_SRC_DIR) $(LLVM_NATIVE_SRC_DIR)

.PHONY: all all-debug all-release clean distclean guestclean

ALWAYS:

guest-tools klee klee-asan llvm llvm-instr-asan llvm-native qemu-debug qemu-debug-asan qemu-release qemu-release-asan stamps tools:
	mkdir -p $@



#############
# Downloads #
#############

LLVM_SRC_URL = http://llvm.org/releases/$(LLVM_VERSION)/

$(CLANG_SRC) $(COMPILER_RT_SRC) $(LLVM_SRC):
	wget $(LLVM_SRC_URL)$@

.INTERMEDIATE: $(CLANG_SRC_DIR) $(COMPILER_RT_SRC_DIR)
$(CLANG_SRC_DIR): $(CLANG_SRC)
$(COMPILER_RT_SRC_DIR): $(COMPILER_RT_SRC)
$(LLVM_SRC_DIR): $(LLVM_SRC)
$(CLANG_SRC_DIR) $(COMPILER_RT_SRC_DIR) $(LLVM_SRC_DIR):
	tar -xmzf $<

stp stp-asan: $(S2ESRC)/stp
$(LLVM_NATIVE_SRC_DIR): $(LLVM_SRC_DIR)
$(LLVM_NATIVE_SRC_DIR) stp stp-asan:
	cp -r $< $@

$(CLANG_DEST_DIR): $(CLANG_SRC_DIR) $(LLVM_SRC_DIR)
$(COMPILER_RT_DEST_DIR): $(COMPILER_RT_SRC_DIR) $(LLVM_NATIVE_SRC_DIR)
$(CLANG_DEST_DIR) $(COMPILER_RT_DEST_DIR):
	mv $< $@



########
# LLVM #
########

CLANG_CC=$(S2EBUILD)/llvm-native/Release/bin/clang
CLANG_CXX=$(S2EBUILD)/llvm-native/Release/bin/clang++

LLVM_CONFIGURE_FLAGS = --prefix=$(S2EBUILD)/opt \
                       --enable-jit --enable-optimized \

#First build it with the system's compiler
stamps/llvm-configure-native: $(CLANG_DEST_DIR) $(COMPILER_RT_DEST_DIR) | llvm-native stamps
	cd llvm-native && $(S2EBUILD)/$(LLVM_NATIVE_SRC_DIR)/configure \
		$(LLVM_CONFIGURE_FLAGS) \
		--disable-assertions #compiler-rt won't build if we specify explicit targets...
	touch $@

stamps/llvm-make-release-native: stamps/llvm-configure-native
	cd llvm-native && make ENABLE_OPTIMIZED=1 -j$(JOBS)
	touch $@


#Then, build LLVM with the clang compiler.
#Note that we build LLVM without clang and compiler-rt, because S2E does not need them.
stamps/llvm-configure: stamps/llvm-make-release-native | llvm
	cd llvm && $(S2EBUILD)/$(LLVM_SRC_DIR)/configure \
	$(LLVM_CONFIGURE_FLAGS) \
	--target=x86_64 --enable-targets=x86 \
	CC=$(CLANG_CC) \
	CXX=$(CLANG_CXX)
	touch $@

stamps/llvm-make-debug: stamps/llvm-configure
	cd llvm && make ENABLE_OPTIMIZED=0 REQUIRES_RTTI=1 -j$(JOBS)
	touch $@

stamps/llvm-make-release: stamps/llvm-configure
	cd llvm && make ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1 -j$(JOBS)
	touch $@



#######
# STP #
#######

stamps/stp-configure: | stamps stp
	cd stp && scripts/configure --with-prefix=$(S2EBUILD)/stp --with-fpic --with-g++=$(CLANG_CXX) --with-gcc=$(CLANG_CC) --with-cryptominisat2
	touch $@

stamps/stp-make: stamps/stp-configure ALWAYS
	cd stp && make 
	touch $@

stp/include/stp/c_interface.h: stamps/stp-configure

stp/lib/libstp.a: stamps/stp-make


#ASAN-enabled STP
#XXX: need to fix the STP build to actually use ASAN...

stamps/stp-configure-asan: | stamps stp-asan
	cd stp-asan && scripts/configure --with-prefix=$(S2EBUILD)/stp-asan --with-fpic --with-g++=$(CLANG_CXX) --with-gcc=$(CLANG_CC) --with-address-sanitizer
	touch $@

stamps/stp-make-asan: stamps/stp-configure-asan ALWAYS
	cd stp-asan && make -j$(JOBS)
	touch $@


########
# KLEE #
########

KLEE_CONFIGURE_FLAGS = --prefix=$(S2EBUILD)/opt \
                       --with-llvmsrc=$(S2EBUILD)/$(LLVM_SRC_DIR) \
                       --with-llvmobj=$(S2EBUILD)/llvm \
                       --target=x86_64 --enable-exceptions \
                       CC=$(CLANG_CC) CXX=$(CLANG_CXX)

stamps/klee-configure: stamps/llvm-configure \
                       stp/include/stp/c_interface.h \
                       stp/lib/libstp.a | klee
	cd klee && $(S2ESRC)/klee/configure \
		--with-stp=$(S2EBUILD)/stp \
		$(KLEE_CONFIGURE_FLAGS)
	touch $@

stamps/klee-make-debug: stamps/klee-configure stamps/llvm-make-debug stamps/stp-make ALWAYS
	cd klee && make ENABLE_OPTIMIZED=0 CXXFLAGS="-g -O0" -j$(JOBS)
	touch $@

stamps/klee-make-release: stamps/klee-configure stamps/llvm-make-release stamps/stp-make ALWAYS
	cd klee && make ENABLE_OPTIMIZED=1 -j$(JOBS)
	touch $@


#ASAN-enabled KLEE
ASAN_CXX_LD_FLAGS = CXXFLAGS="-O0 -g -fsanitize=address" LDFLAGS="-g -fsanitize=address"

stamps/klee-configure-asan: stamps/llvm-make-release stamps/stp-make-asan | klee-asan
	cd klee-asan && $(S2ESRC)/klee/configure \
		--with-stp=$(S2EBUILD)/stp-asan \
		$(KLEE_CONFIGURE_FLAGS) \
		$(ASAN_CXX_LD_FLAGS)
	touch $@


stamps/klee-make-release-asan: stamps/klee-configure-asan stamps/stp-make-asan
	cd klee-asan && make ENABLE_OPTIMIZED=1 $(ASAN_CXX_LD_FLAGS) -j$(JOBS)
	touch $@

stamps/klee-make-debug-asan: stamps/llvm-make-debug stamps/klee-configure-asan stamps/stp-make-asan
	cd klee-asan && make ENABLE_OPTIMIZED=0 $(ASAN_CXX_LD_FLAGS) -j$(JOBS)
	touch $@


########
# QEMU #
########

klee/Debug/bin/klee-config: stamps/klee-make-debug

klee/Release/bin/klee-config: stamps/klee-make-release

QEMU_COMMON_FLAGS = --prefix=$(S2EBUILD)/opt\
                    --cc=$(CLANG_CC) \
                    --cxx=$(CLANG_CXX) \
                    --target-list=x86_64-s2e-softmmu,x86_64-softmmu,i386-s2e-softmmu,i386-softmmu,arm-s2e-softmmu,arm-softmmu \
                    --enable-llvm \
                    --enable-s2e \
                    --with-pkgversion=S2E \
                    $(EXTRA_QEMU_FLAGS)

QEMU_CONFIGURE_FLAGS = --with-stp=$(S2EBUILD)/stp \
                       $(QEMU_COMMON_FLAGS)

QEMU_DEBUG_FLAGS = --with-llvm=$(S2EBUILD)/llvm/Debug+Asserts \
                   --enable-debug

QEMU_RELEASE_FLAGS = --with-llvm=$(S2EBUILD)/llvm/Release+Asserts

stamps/qemu-configure-debug: stamps/klee-configure klee/Debug/bin/klee-config | qemu-debug
	cd qemu-debug && $(S2ESRC)/qemu/configure \
		--with-klee=$(S2EBUILD)/klee/Debug+Asserts \
		$(QEMU_DEBUG_FLAGS) \
		$(QEMU_CONFIGURE_FLAGS)
	touch $@

stamps/qemu-configure-release: stamps/klee-configure klee/Release/bin/klee-config | qemu-release
	cd qemu-release && $(S2ESRC)/qemu/configure \
		--with-klee=$(S2EBUILD)/klee/Release+Asserts \
		$(QEMU_RELEASE_FLAGS) \
		$(QEMU_CONFIGURE_FLAGS)
	touch $@

stamps/qemu-make-debug: stamps/qemu-configure-debug stamps/klee-make-debug
	cd qemu-debug && make -j$(JOBS)
	touch $@

stamps/qemu-make-release: stamps/qemu-configure-release stamps/klee-make-release
	cd qemu-release && make -j$(JOBS)
	touch $@

#ASAN-enabled QEMU
QEMU_ASAN_FLAGS = --enable-address-sanitizer \
                  --with-stp=$(S2EBUILD)/stp \
                  $(QEMU_COMMON_FLAGS)

stamps/qemu-configure-release-asan: stamps/klee-make-release-asan | qemu-release-asan
	cd qemu-release-asan && $(S2ESRC)/qemu/configure \
		--with-klee=$(S2EBUILD)/klee-asan/Release+Asserts \
		$(QEMU_RELEASE_FLAGS) \
		$(QEMU_ASAN_FLAGS)
	touch $@

stamps/qemu-configure-debug-asan: stamps/klee-make-debug-asan | qemu-debug-asan
	cd qemu-debug-asan && $(S2ESRC)/qemu/configure \
		--with-klee=$(S2EBUILD)/klee-asan/Debug+Asserts \
		$(QEMU_DEBUG_FLAGS) \
		$(QEMU_ASAN_FLAGS)
	touch $@

stamps/qemu-make-debug-asan: stamps/qemu-configure-debug-asan stamps/klee-make-debug-asan
	cd qemu-debug-asan && make -j$(JOBS)
	touch $@

stamps/qemu-make-release-asan: stamps/qemu-configure-release-asan stamps/klee-make-release-asan
	cd qemu-release-asan && make -j$(JOBS)
	touch $@



#########
# Tools #
#########

stamps/tools-configure: stamps/llvm-configure | tools
	cd tools && $(S2ESRC)/tools/configure \
		--with-llvmsrc=$(S2EBUILD)/$(LLVM_SRC_DIR) \
		--with-llvmobj=$(S2EBUILD)/llvm \
		--with-s2esrc=$(S2ESRC)/qemu \
		--target=x86_64 \
		CC=$(CLANG_CC) \
		CXX=$(CLANG_CXX)
	touch $@

stamps/tools-make-release: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1 -j$(JOBS)
	touch $@

stamps/tools-make-debug: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=0 REQUIRES_RTTI=1 -j$(JOBS)
	touch $@


############
#Guest tools
############

stamps/guest-tools: ALWAYS | guest-tools stamps
	cd $(S2ESRC)/guest && make install BUILD_DIR=$(S2EBUILD)/guest-tools
	touch $@
