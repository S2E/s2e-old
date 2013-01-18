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
    $(error You should not run make in the S2E source directory!)
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


# The following fetches and builds Address Sanitizer
stamps/llvm-trunk-download:
	svn co http://llvm.org/svn/llvm-project/llvm/trunk llvm-asan
	mkdir -p stamps && touch $@

stamps/llvm-trunk-revision: stamps/llvm-trunk-download
	cd llvm-asan && svn info | grep Revision: | awk '{print $$2}' > ../$@


stamps/clang-trunk: stamps/llvm-trunk-revision
	(cd llvm-asan/tools && svn co -r `cat ../../stamps/llvm-trunk-revision` http://llvm.org/svn/llvm-project/cfe/trunk clang)
	mkdir -p stamps && touch $@

stamps/compilerrt-trunk: stamps/clang-trunk
	(cd llvm-asan/projects && svn co -r `cat ../../stamps/llvm-trunk-revision` http://llvm.org/svn/llvm-project/compiler-rt/trunk compiler-rt)
	mkdir -p stamps && touch $@

stamps/llvm-trunk-build: stamps/compilerrt-trunk
	(mkdir -p llvm-asan/build && cd llvm-asan/build && ../configure --enable-targets=x86 --enable-optimized && make -j$(JOBS))
	mkdir -p stamps && touch $@

stamps/asan-get-thirdparty: stamps/llvm-trunk-build
	cd llvm-asan/projects/compiler-rt/lib/asan && make -f Makefile.old get_third_party
	mkdir -p stamps && touch $@

stamps/asan-makeinstall: stamps/asan-get-thirdparty
	cd llvm-asan/projects/compiler-rt/lib/asan && make -f Makefile.old -j$(JOBS)
	cd llvm-asan/projects/compiler-rt/lib/asan && make -f Makefile.old install -j$(JOBS)
	cp llvm-asan/projects/compiler-rt/lib/asan_clang_linux/lib/clang/linux/x86_64/libclang_rt.asan.a llvm-asan/projects/compiler-rt/lib/asan_clang_linux/lib/libasan64.a
	mkdir -p stamps && touch $@

ASAN_DIR=$(S2EBUILD)/llvm-asan/projects/compiler-rt/lib/asan_clang_linux
ASAN_CC=$(ASAN_DIR)/bin/clang
ASAN_CXX=$(ASAN_DIR)/bin/clang++

CLANG_CC=$(S2EBUILD)/llvm-native/Release/bin/clang
CLANG_CXX=$(S2EBUILD)/llvm-native/Release/bin/clang++

########
# LLVM #
########


#First build it with the system's compiler
stamps/llvm-configure-native: stamps/clang-unpack stamps/llvm-unpack
	mkdir -p llvm-native
	echo $(S2EBUILD) $(S2ESRC)
	cd llvm-native && $(S2EBUILD)/$(LLVM_SRC_DIR)/configure \
		--prefix=$(S2EBUILD)/opt \
		--target=x86_64 --enable-targets=x86 --enable-jit \
		--enable-optimized
	mkdir -p stamps && touch $@

stamps/llvm-make-release-native: stamps/llvm-configure-native
	cd llvm-native && make ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1 -j$(JOBS)
	mkdir -p stamps && touch $@


#Then, build it with the clang compiler.
stamps/llvm-configure: stamps/llvm-make-release-native
	mkdir -p llvm
	cd llvm && $(S2EBUILD)/$(LLVM_SRC_DIR)/configure \
		--prefix=$(S2EBUILD)/opt \
		--target=x86_64 --enable-targets=x86 --enable-jit \
		--enable-optimized --enable-assertions\
		CC=$(S2EBUILD)/llvm-native/Release/bin/clang \
		CXX=$(S2EBUILD)/llvm-native/Release/bin/clang++
	mkdir -p stamps && touch $@

stamps/llvm-make-debug: stamps/llvm-configure
	cd llvm && make ENABLE_OPTIMIZED=0 REQUIRES_RTTI=1 -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/llvm-make-release: stamps/llvm-configure
	cd llvm && make ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1 -j$(JOBS)
	mkdir -p stamps && touch $@

#This is for building it with ASAN
stamps/llvm-configure-asan: stamps/asan-makeinstall
	mkdir -p llvm-instr-asan
	cd llvm-instr-asan && $(S2EBUILD)/$(LLVM_SRC_DIR)/configure \
		--prefix=$(S2EBUILD)/opt \
		--target=x86_64 --enable-targets=x86 --enable-jit \
		--enable-optimized --enable-assertions\
		CC=$(ASAN_CC) \
		CXX=$(ASAN_CXX) CXXFLAGS="-O1 -faddress-sanitizer" LDFLAGS="-faddress-sanitizer"
	mkdir -p stamps && touch $@

stamps/llvm-make-release-asan: stamps/llvm-configure-asan
	-(cd llvm-instr-asan && make ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1 CXXFLAGS="-O1 -faddress-sanitizer" -j$(JOBS))
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

#ASAN-enabled STP

stamps/stp-copy-asan:
	cp -Rup $(S2ESRC)/stp stp-asan
	mkdir -p stamps && touch $@

stamps/stp-configure-asan: stamps/stp-copy-asan
	cd stp-asan && bash scripts/configure --with-prefix=$(S2EBUILD)/stp-asan --with-g++=$(ASAN_CXX) --with-gcc=$(ASAN_CC) --with-address-sanitizer
	cd stp-asan && cp src/c_interface/c_interface.h include/stp
	mkdir -p stamps && touch $@

stamps/stp-make-asan: stamps/stp-configure-asan ALWAYS
	cp -Rup $(S2ESRC)/stp stp-asan
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
		--enable-exceptions --enable-assertions \
                CC=$(S2EBUILD)/llvm-native/Release/bin/clang \
		CXX=$(S2EBUILD)/llvm-native/Release/bin/clang++
	mkdir -p stamps && touch $@

stamps/klee-make-debug: stamps/klee-configure stamps/llvm-make-debug stamps/stp-make ALWAYS
	cd klee && make ENABLE_OPTIMIZED=0 CXXFLAGS="-g -O0" -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/klee-make-release: stamps/klee-configure stamps/llvm-make-release stamps/stp-make ALWAYS
	cd klee && make ENABLE_OPTIMIZED=1 -j$(JOBS)
	mkdir -p stamps && touch $@

#ASAN-enabled KLEE
stamps/klee-configure-asan: stamps/llvm-make-release-asan stamps/stp-make-asan \
                       stp/include/stp/c_interface.h \
		       stp/lib/libstp.a
	mkdir -p klee-asan
	cd klee-asan && $(S2ESRC)/klee/configure \
		--prefix=$(S2EBUILD)/opt \
		--with-llvmsrc=$(S2EBUILD)/$(LLVM_SRC_DIR) \
		--with-llvmobj=$(S2EBUILD)/llvm-instr-asan \
		--with-stp=$(S2EBUILD)/stp-asan \
		--target=x86_64 \
		--enable-exceptions --enable-assertions \
                CC=$(ASAN_CC) \
		CXX=$(ASAN_CXX) CXXFLAGS="-O1 -faddress-sanitizer" LDFLAGS="-faddress-sanitizer"
	mkdir -p stamps && touch $@

stamps/klee-make-release-asan: stamps/klee-configure-asan stamps/stp-make ALWAYS
	cd klee-asan && make ENABLE_OPTIMIZED=1 CXXFLAGS="-O1 -faddress-sanitizer" LDFLAGS="-faddress-sanitizer"  -j$(JOBS)
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
		--with-clang=$(S2EBUILD)/llvm-native/Release/bin/clang \
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
		--with-clang=$(S2EBUILD)/llvm-native/Release/bin/clang \
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

#ASAN-enabled QEMU
stamps/qemu-configure-release-asan: stamps/klee-make-release-asan
	mkdir -p qemu-release-asan
	cd qemu-release-asan && $(S2ESRC)/qemu/configure \
		--prefix=$(S2EBUILD)/opt \
		--with-llvm=$(S2EBUILD)/llvm-instr-asan/Release+Asserts  \
		--with-clang=$(S2EBUILD)/llvm-native/Release/bin/clang \
		--with-stp=$(S2EBUILD)/stp-asan \
		--with-klee=$(S2EBUILD)/klee/Release+Asserts \
		--target-list=i386-s2e-softmmu,i386-softmmu \
		--asancc=$(ASAN_CC) \
		--asancxx=$(ASAN_CXX) \
		--enable-llvm \
		--enable-s2e --enable-address-sanitizer --compile-all-with-clang
	mkdir -p stamps && touch $@

stamps/qemu-make-release-asan: stamps/qemu-configure-release-asan stamps/klee-make-release-asan ALWAYS
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
		--target=x86_64 --enable-assertions \
		CC=$(S2EBUILD)/llvm-native/Release/bin/clang \
		CXX=$(S2EBUILD)/llvm-native/Release/bin/clang++
	mkdir -p stamps && touch $@

stamps/tools-make-release: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1 -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/tools-make-debug: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=0 REQUIRES_RTTI=1 -j$(JOBS)
	mkdir -p stamps && touch $@

