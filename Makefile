SRCDIR:=$(PWD)/../s2e
BUILDDIR:=$(PWD)
JOBS:=8

all: all-release

all-release: stamps/qemu-make-release stamps/tools-make-release

all-debug: stamps/qemu-make-debug stamps/tools-make-debug

clean:
	rm -Rf tools qemu-release qemu-debug klee stp llvm
	rm -Rf llvm-2.6
	rm -Rf llvm-gcc-4.2-2.6-x86_64-linux
	rm -Rf stamps

.PHONY: all all-debug all-release clean

ALWAYS:

#############
# Downloads #
#############

llvm-gcc-4.2-2.6-x86_64-linux.tar.gz:
	wget http://llvm.org/releases/2.6/llvm-gcc-4.2-2.6-x86_64-linux.tar.gz

stamps/llvm-gcc-unpack: llvm-gcc-4.2-2.6-x86_64-linux.tar.gz
	tar -zxf llvm-gcc-4.2-2.6-x86_64-linux.tar.gz
	mkdir -p stamps && touch $@

llvm-2.6.tar.gz:
	wget http://llvm.org/releases/2.6/llvm-2.6.tar.gz

stamps/llvm-unpack: llvm-2.6.tar.gz
	tar -zxf llvm-2.6.tar.gz
	mkdir -p stamps && touch $@

########
# LLVM #
########

stamps/llvm-configure: stamps/llvm-gcc-unpack stamps/llvm-unpack
	mkdir -p llvm
	cd llvm && $(BUILDDIR)/llvm-2.6/configure \
		--prefix=$(BUILDDIR)/opt \
		--with-llvmgccdir=$(BUILDDIR)/llvm-gcc-4.2-2.6-x86_64-linux \
		--enable-optimized
	mkdir -p stamps && touch $@

stamps/llvm-make-debug: stamps/llvm-configure
	cd llvm && make ENABLE_OPTIMIZED=0 -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/llvm-make-release: stamps/llvm-configure
	cd llvm && make ENABLE_OPTIMIZED=1 -j$(JOBS)
	mkdir -p stamps && touch $@

#######
# STP #
#######

stamps/stp-copy:
	cp -Rfp $(SRCDIR)/stp stp
	mkdir -p stamps && touch $@

stamps/stp-configure: stamps/stp-copy
	cd stp && bash scripts/configure --with-prefix=$(BUILDDIR)/stp
	cd stp && cp src/c_interface/c_interface.h include/stp
	mkdir -p stamps && touch $@

stamps/stp-make: stamps/stp-configure ALWAYS
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
	cd klee && $(SRCDIR)/klee/configure \
		--prefix=$(BUILDDIR)/opt \
		--with-llvmsrc=$(BUILDDIR)/llvm-2.6 \
		--with-llvmobj=$(BUILDDIR)/llvm \
		--with-stp=$(BUILDDIR)/stp \
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
	cd qemu-debug && $(SRCDIR)/qemu/configure \
		--prefix=$(BUILDDIR)/opt \
		--with-llvm=$(BUILDDIR)/llvm/Debug  \
		--with-llvmgcc=$(BUILDDIR)/llvm-gcc-4.2-2.6-x86_64-linux/bin/llvm-gcc \
		--with-stp=$(BUILDDIR)/stp \
		--with-klee=$(BUILDDIR)/klee/Debug \
		--target-list=i386-s2e-softmmu,i386-softmmu \
		--enable-llvm \
		--enable-s2e \
		--enable-debug
	mkdir -p stamps && touch $@

stamps/qemu-configure-release: stamps/klee-configure klee/Release/bin/klee-config
	mkdir -p qemu-release
	cd qemu-release && $(SRCDIR)/qemu/configure \
		--prefix=$(BUILDDIR)/opt \
		--with-llvm=$(BUILDDIR)/llvm/Release  \
		--with-llvmgcc=$(BUILDDIR)/llvm-gcc-4.2-2.6-x86_64-linux/bin/llvm-gcc \
		--with-stp=$(BUILDDIR)/stp \
		--with-klee=$(BUILDDIR)/klee/Release \
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
	cd tools && $(SRCDIR)/tools/configure \
		--with-llvmsrc=$(BUILDDIR)/llvm-2.6 \
		--with-llvmobj=$(BUILDDIR)/llvm \
		--with-s2esrc=$(SRCDIR)/qemu
	mkdir -p stamps && touch $@

stamps/tools-make-release: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=1 -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/tools-make-debug: stamps/tools-configure ALWAYS
	cd tools-debug && make ENABLE_OPTIMIZED=0 -j$(JOBS)
	mkdir -p stamps && touch $@

