S2ESRC:=$(CURDIR)/..
S2EBUILD:=$(CURDIR)

JOBS:=8

all: checkperms all-release

all-release: stamps/android-make-release stamps/android-make-release-nos2e stamps/tools-make-release

all-debug: stamps/android-make-debug stamps/android-make-debug-nos2e stamps/tools-make-debug

checkperms:
	chmod +x ../android-emulator/android/tools/gen-hw-config.py
	chmod +x ../android-emulator/hxtool
	chmod +x ../android-emulator/feature_to_c.sh
	chmod +x ../android-emulator/android/build/mkdeps.sh
	chmod +x ../android-emulator/android-configure.sh
	chmod +x ../klee/configure
	chmod +x ../stp/src/AST/genkinds.pl
	chmod +x ../tools/configure

ifeq ($(shell ls qemu/vl.c 2>&1),qemu/vl.c)
    $(error You should not run make in S2E source directory!)
endif

ifeq ($(shell uname -s),Darwin)
    LLVM_GCC_SRC=llvm-gcc-4.2-2.6-i386-darwin9
else
    LLVM_GCC_SRC=llvm-gcc-4.2-2.6-x86_64-linux
endif

clean-android:
	rm -Rf $(S2ESRC)/android-emulator/objs
	rm -Rf stamps/android-configure-debug

clean: clean-android
	rm -Rf tools android-release android-debug klee stp llvm
	rm -Rf llvm-2.6
	rm -Rf $(LLVM_GCC_SRC)
	rm -Rf stamps

.PHONY: all all-debug all-release clean

ALWAYS:

#############
# Downloads #
#############

$(LLVM_GCC_SRC).tar.gz:
	wget http://llvm.org/releases/2.6/$(LLVM_GCC_SRC).tar.gz

stamps/llvm-gcc-unpack: $(LLVM_GCC_SRC).tar.gz
	tar -zxf $(LLVM_GCC_SRC).tar.gz
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
	cd llvm && $(S2EBUILD)/llvm-2.6/configure \
		--prefix=$(S2EBUILD)/opt \
		--with-llvmgccdir=$(S2EBUILD)/$(LLVM_GCC_SRC) \
		--target=x86_64 \
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
		--with-llvmsrc=$(S2EBUILD)/llvm-2.6 \
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

####################
# Android Emulator #
####################

klee/Debug/bin/klee-config: stamps/klee-make-debug

klee/Release/bin/klee-config: stamps/klee-make-release

stamps/android-configure-debug: clean-android stamps/klee-configure klee/Debug/bin/klee-config
	mkdir -p android-debug
	cd android-debug && $(S2ESRC)/android-emulator/android-configure.sh \
		--with-llvm=$(S2EBUILD)/llvm/Debug  \
		--with-llvmgcc=$(S2EBUILD)/$(LLVM_GCC_SRC)/bin/llvm-gcc \
		--with-stp=$(S2EBUILD)/stp \
		--with-klee=$(S2EBUILD)/klee/Debug \
		--enable-llvm \
		--enable-s2e \
		--debug \
		--zero-malloc \
		--try-64 \
		--sdl-config=/home/aka/android-sdl/bin/sdl-config \
		--install=$(S2EBUILD)/android-debug 
	mkdir -p stamps && touch $@

stamps/android-configure-release: clean-android stamps/klee-configure klee/Release/bin/klee-config
	mkdir -p android-release
	cd android-release && $(S2ESRC)/android-emulator/android-configure.sh \
		--with-llvm=$(S2EBUILD)/llvm/Release  \
		--with-llvmgcc=$(S2EBUILD)/$(LLVM_GCC_SRC)/bin/llvm-gcc \
		--with-stp=$(S2EBUILD)/stp \
		--with-klee=$(S2EBUILD)/klee/Release \
		--enable-llvm \
		--enable-s2e \
		--zero-malloc \
		--try-64 \
		--sdl-config=/home/aka/android-sdl/bin/sdl-config \
		--install=$(S2EBUILD)/android-release

	mkdir -p stamps && touch $@

stamps/android-make-debug: stamps/android-configure-debug stamps/klee-make-debug ALWAYS
	cd $(S2ESRC)/android-emulator && make -j$(JOBS)
	mkdir -p stamps && touch $@
	cp -R $(S2ESRC)/android-emulator/objs $(S2EBUILD)/android-debug
 
stamps/android-make-release: stamps/android-configure-release stamps/klee-make-release ALWAYS
	cd $(S2ESRC)/android-emulator && make -j$(JOBS)
	mkdir -p stamps && touch $@
	cp -R $(S2ESRC)/android-emulator/objs $(S2EBUILD)/android-release


###################################
# Android Emulator (S2E disabled) #
###################################

stamps/android-configure-debug-nos2e: clean-android stamps/klee-configure klee/Debug/bin/klee-config
	mkdir -p android-debug
	cd android-debug && $(S2ESRC)/android-emulator/android-configure.sh \
		--debug \
		--zero-malloc \
		--try-64 \
		--sdl-config=/home/aka/android-sdl/bin/sdl-config \
		--install=$(S2EBUILD)/android-debug 
	mkdir -p stamps && touch $@

stamps/android-configure-release-nos2e: clean-android stamps/klee-configure klee/Release/bin/klee-config
	mkdir -p android-release
	cd android-release && $(S2ESRC)/android-emulator/android-configure.sh \
		--zero-malloc \
		--try-64 \
		--sdl-config=/home/aka/android-sdl/bin/sdl-config \
		--install=$(S2EBUILD)/android-release

	mkdir -p stamps && touch $@

stamps/android-make-debug-nos2e: stamps/android-configure-debug-nos2e stamps/klee-make-debug ALWAYS
	cd $(S2ESRC)/android-emulator && make -j$(JOBS)
	mkdir -p stamps && touch $@
	mkdir -p $(S2EBUILD)/android-debug/objs-nos2e
	cp -R $(S2ESRC)/android-emulator/objs/* $(S2EBUILD)/android-debug/objs-nos2e/
 
stamps/android-make-release-nos2e: stamps/android-configure-release-nos2e stamps/klee-make-release ALWAYS
	cd $(S2ESRC)/android-emulator && make -j$(JOBS)
	mkdir -p stamps && touch $@
	mkdir -p $(S2EBUILD)/android-release/objs-nos2e
	cp -R $(S2ESRC)/android-emulator/objs/* $(S2EBUILD)/android-release/objs-nos2e/


#########
# Tools #
#########

stamps/tools-configure: stamps/llvm-configure
	mkdir -p tools
	cd tools && $(S2ESRC)/tools/configure \
		--with-llvmsrc=$(S2EBUILD)/llvm-2.6 \
		--with-llvmobj=$(S2EBUILD)/llvm \
		--with-s2esrc=$(S2ESRC)/android-emulator \
		--target=x86_64
	mkdir -p stamps && touch $@

stamps/tools-make-release: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=1 -j$(JOBS)
	mkdir -p stamps && touch $@

stamps/tools-make-debug: stamps/tools-configure ALWAYS
	cd tools && make ENABLE_OPTIMIZED=0 -j$(JOBS)
	mkdir -p stamps && touch $@

