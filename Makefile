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
# PARALLEL=no
#      Turn off build parallelization.

S2ESRC := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
S2EBUILD:=$(CURDIR)

OS := $(shell uname)
JOBS:=2

ifeq ($(PARALLEL),no)
JOBS := 1
else ifeq ($(OS),Darwin)
JOBS := $(patsubst hw.ncpu:%,%,$(shell sysctl hw.ncpu))
else ifeq ($(OS),Linux)
JOBS := $(shell grep -c ^processor /proc/cpuinfo)
endif
MAKE = make -j$(JOBS)

all: all-release

all-release all-asan-release: stamps/tools-release-make
all-release: stamps/qemu-release-make
all-asan-release: stamps/qemu-asan-release-make

all-debug all-asan-debug: stamps/tools-debug-make
all-debug: stamps/qemu-debug-make
all-asan-debug: stamps/qemu-asan-debug-make

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

KLEE_QEMU_DIRS = $(foreach suffix,-debug -release,$(addsuffix $(suffix),klee klee-asan qemu qemu-asan))

clean:
	-rm -Rf $(KLEE_QEMU_DIRS)
	-rm -Rf stamps

guestclean:
	-$(MAKE) -C $(S2ESRC)/guest clean
	-rm -f stamps/guest-tools

distclean: clean guestclean
	-rm -Rf guest-tools llvm llvm-native stp stp-asan tools
	-rm -Rf $(COMPILER_RT_SRC_DIR) $(LLVM_SRC_DIR) $(LLVM_NATIVE_SRC_DIR)

.PHONY: all all-debug all-release all-asan-debug all-asan-release
.PHONY: clean distclean guestclean

ALWAYS:

guest-tools $(KLEE_QEMU_DIRS) llvm llvm-native stamps tools:
	mkdir -p $@

stamps/%-configure: | % stamps
	cd $* && $(CONFIGURE_COMMAND)
	touch $@



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
stamps/llvm-native-configure: $(CLANG_DEST_DIR) $(COMPILER_RT_DEST_DIR)
stamps/llvm-native-configure: CONFIGURE_COMMAND = $(S2EBUILD)/$(LLVM_NATIVE_SRC_DIR)/configure \
                                                  $(LLVM_CONFIGURE_FLAGS) \
                                                  --disable-assertions #compiler-rt won't build if we specify explicit targets...

$(CLANG_CXX): stamps/llvm-native-configure
	$(MAKE) -C llvm-native ENABLE_OPTIMIZED=1


#Then, build LLVM with the clang compiler.
#Note that we build LLVM without clang and compiler-rt, because S2E does not need them.
stamps/llvm-configure: $(CLANG_CXX)
stamps/llvm-configure: CONFIGURE_COMMAND = $(S2EBUILD)/$(LLVM_SRC_DIR)/configure \
                                           $(LLVM_CONFIGURE_FLAGS) \
                                           --target=x86_64 --enable-targets=x86 \
                                           CC=$(CLANG_CC) \
                                           CXX=$(CLANG_CXX)

stamps/llvm-debug-make stamps/llvm-release-make: stamps/llvm-configure

stamps/llvm-debug-make:
	$(MAKE) -C llvm ENABLE_OPTIMIZED=0 REQUIRES_RTTI=1
	touch $@

stamps/llvm-release-make:
	$(MAKE) -C llvm ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1
	touch $@



#######
# STP #
#######

stamps/stp-make stamps/stp-asan-make: $(CLANG_CXX) ALWAYS

STP_CONFIGURE_FLAGS = --with-prefix=$(S2EBUILD)/stp --with-fpic \
                      --with-g++=$(CLANG_CXX) --with-gcc=$(CLANG_CC) \
                      --with-cryptominisat2

stamps/stp-configure: CONFIGURE_COMMAND = scripts/configure $(STP_CONFIGURE_FLAGS)

stamps/stp-make: stamps/stp-configure
	$(MAKE) -C stp
	touch $@

#ASAN-enabled STP
#XXX: need to fix the STP build to actually use ASAN...

stamps/stp-asan-configure: CONFIGURE_COMMAND = scripts/configure $(STP_CONFIGURE_FLAGS) --with-address-sanitizer

stamps/stp-asan-make: stamps/stp-asan-configure
	$(MAKE) -C stp-asan
	touch $@


########
# KLEE #
########

stamps/klee-debug-make stamps/klee-asan-debug-make: stamps/llvm-debug-make
stamps/klee-release-make stamps/klee-asan-release-make: stamps/llvm-release-make
stamps/klee-debug-make stamps/klee-asan-debug-make stamps/klee-release-make stamps/klee-asan-release-make: ALWAYS
stamps/klee-debug-make stamps/klee-release-make: stamps/stp-make
stamps/klee-debug-make:   stamps/klee-debug-configure
stamps/klee-release-make: stamps/klee-release-configure
stamps/klee-asan-debug-make stamps/klee-asan-release-make: stamps/stp-asan-make
stamps/klee-asan-debug-make:   stamps/klee-asan-debug-configure
stamps/klee-asan-release-make: stamps/klee-asan-release-configure

KLEE_CONFIGURE_COMMON = $(S2ESRC)/klee/configure \
                        --prefix=$(S2EBUILD)/opt \
                        --with-llvmsrc=$(S2EBUILD)/$(LLVM_SRC_DIR) \
                        --with-llvmobj=$(S2EBUILD)/llvm \
                        --target=x86_64 --enable-exceptions \
                        CC=$(CLANG_CC) CXX=$(CLANG_CXX)

KLEE_CONFIGURE_COMMAND = $(KLEE_CONFIGURE_COMMON) --with-stp=$(S2EBUILD)/stp

stamps/klee-debug-configure: CONFIGURE_COMMAND = $(KLEE_CONFIGURE_COMMAND) \
                                                 CXXFLAGS="-g -O0" LDFLAGS="-g"

stamps/klee-release-configure: CONFIGURE_COMMAND = $(KLEE_CONFIGURE_COMMAND)

stamps/klee-debug-make:
	$(MAKE) -C klee-debug ENABLE_OPTIMIZED=0
	touch $@

stamps/klee-release-make:
	$(MAKE) -C klee-release ENABLE_OPTIMIZED=1
	touch $@

#ASAN-enabled KLEE

KLEE_ASAN_CONFIGURE_COMMAND = $(KLEE_CONFIGURE_COMMON) --with-stp=$(S2EBUILD)/stp-asan

stamps/klee-asan-debug-configure: CONFIGURE_COMMAND = $(KLEE_ASAN_CONFIGURE_COMMAND) \
                                                      CXXFLAGS="-g -O0 -fsanitize=address" \
                                                      LDFLAGS="-g -fsanitize=address"

stamps/klee-asan-release-configure: CONFIGURE_COMMAND = $(KLEE_ASAN_CONFIGURE_COMMAND) \
                                                        CXXFLAGS="-fsanitize=address" \
                                                        LDFLAGS="-fsanitize=address"

stamps/klee-asan-debug-make:
	$(MAKE) -C klee-asan-debug ENABLE_OPTIMIZED=0
	touch $@

stamps/klee-asan-release-make:
	$(MAKE) -C klee-asan-release ENABLE_OPTIMIZED=1
	touch $@


########
# QEMU #
########

QEMU_COMMON_FLAGS = --prefix=$(S2EBUILD)/opt\
                    --cc=$(CLANG_CC) \
                    --cxx=$(CLANG_CXX) \
                    --target-list=x86_64-s2e-softmmu,x86_64-softmmu,i386-s2e-softmmu,i386-softmmu \
                    --enable-llvm \
                    --enable-s2e \
                    --with-pkgversion=S2E \
                    $(EXTRA_QEMU_FLAGS)

QEMU_CONFIGURE_FLAGS = --with-stp=$(S2EBUILD)/stp \
                       $(QEMU_COMMON_FLAGS)

QEMU_DEBUG_FLAGS = --with-llvm=$(S2EBUILD)/llvm/Debug+Asserts \
                   --enable-debug

QEMU_RELEASE_FLAGS = --with-llvm=$(S2EBUILD)/llvm/Release+Asserts

stamps/qemu-debug-configure: CONFIGURE_COMMAND = $(S2ESRC)/qemu/configure \
                                                 --with-klee=$(S2EBUILD)/klee-debug/Debug+Asserts \
                                                 $(QEMU_DEBUG_FLAGS) \
                                                 $(QEMU_CONFIGURE_FLAGS)

stamps/qemu-release-configure: CONFIGURE_COMMAND = $(S2ESRC)/qemu/configure \
                                                   --with-klee=$(S2EBUILD)/klee-release/Release+Asserts \
                                                   $(QEMU_RELEASE_FLAGS) \
                                                   $(QEMU_CONFIGURE_FLAGS)

stamps/qemu-debug-make: stamps/klee-debug-make stamps/qemu-debug-configure
	$(MAKE) -C qemu-debug
	touch $@

stamps/qemu-release-make: stamps/klee-release-make stamps/qemu-release-configure
	$(MAKE) -C qemu-release
	touch $@

#ASAN-enabled QEMU
QEMU_ASAN_FLAGS = --enable-address-sanitizer \
                  --with-stp=$(S2EBUILD)/stp-asan \
                  $(QEMU_COMMON_FLAGS)

stamps/qemu-asan-release-configure: CONFIGURE_COMMAND = $(S2ESRC)/qemu/configure \
                                                        --with-klee=$(S2EBUILD)/klee-asan-release/Release+Asserts \
                                                        $(QEMU_RELEASE_FLAGS) \
                                                        $(QEMU_ASAN_FLAGS)

stamps/qemu-asan-debug-configure: CONFIGURE_COMMAND = $(S2ESRC)/qemu/configure \
                                                      --with-klee=$(S2EBUILD)/klee-asan-debug/Debug+Asserts \
                                                      $(QEMU_DEBUG_FLAGS) \
                                                      $(QEMU_ASAN_FLAGS)

stamps/qemu-asan-debug-make: stamps/klee-asan-debug-make stamps/qemu-asan-debug-configure
	$(MAKE) -C qemu-asan-debug
	touch $@

stamps/qemu-asan-release-make: stamps/klee-asan-release-make stamps/qemu-asan-release-configure
	$(MAKE) -C qemu-asan-release
	touch $@



#########
# Tools #
#########

stamps/tools-release-make: stamps/llvm-release-make
stamps/tools-debug-make: stamps/llvm-debug-make
stamps/tools-debug-make stamps/tools-release-make: stamps/tools-configure ALWAYS

stamps/tools-configure: CONFIGURE_COMMAND = $(S2ESRC)/tools/configure \
                                            --with-llvmsrc=$(S2EBUILD)/$(LLVM_SRC_DIR) \
                                            --with-llvmobj=$(S2EBUILD)/llvm \
                                            --with-s2esrc=$(S2ESRC)/qemu \
                                            --target=x86_64 CC=$(CLANG_CC) CXX=$(CLANG_CXX)

stamps/tools-release-make:
	$(MAKE) -C tools ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1
	touch $@

stamps/tools-debug-make:
	$(MAKE) -C tools ENABLE_OPTIMIZED=0 REQUIRES_RTTI=1
	touch $@



############
#Guest tools
############

stamps/guest-tools: ALWAYS | guest-tools stamps
	$(MAKE) -C $(S2ESRC)/guest install BUILD_DIR=$(S2EBUILD)/guest-tools
	touch $@
