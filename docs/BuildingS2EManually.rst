===================================
Building the S2E Framework Manually
===================================

.. contents::

Before following this guide you should install dependencies as described in
`Building S2E <BuildingS2E.html>`_.

Building LLVM 3.0 with ``clang``
================================

In order to build S2E you must compile LLVM from sources. LLVM installed from
binary distribution packages will not work.

S2E and LLVM are built using the ``clang``. Since ``clang`` itself relies
on LLVM, we first build LLVM and ``clang`` using ``gcc``. Then, we rebuild
LLVM a second time using the ``clang`` compiler. In a nutshell, ``gcc`` is
only used to bootstrap the process.


::

   $ S2ESRC=/path/to/s2e
   $ S2EBUILD=/path/to/build

   $ cd $S2EBUILD

   $ # Download and unpack clang and llvm-3.0
   $ wget http://llvm.org/releases/3.0/clang-3.0.tar.gz
   $ wget http://llvm.org/releases/3.0/llvm-3.0.tar.gz
   $ tar -zxf clang-3.0.tar.gz
   $ tar -zxf llvm-3.0.tar.gz
   $ # Move clang to the LLVM's project directory
   $ mv clang-3.0.src llvm-3.0.src/tools/clang

   $ #First, build LLVM and clang with the system's compiler
   $ mkdir $S2EBUILD/llvm-native
   $ cd $S2EBUILD/llvm-native
   $ ../llvm-3.0.src/configure \
       --prefix=$S2EBUILD/opt \
       --enable-optimized --target=x86_64 --enable-targets=x86 --enable-jit

   $ # Compile release build of the native compiler.
   $ # Note that it is not necessary to have a debug version of it.
   $ make ENABLE_OPTIMIZED=1 -j4

   $ # Second, build again but this time with clang.
   $ # This is the compiled LLVM version that will be used by S2E
   $ mkdir $S2EBUILD/llvm
   $ cd $S2EBUILD/llvm
   $ ../llvm-3.0.src/configure \
       --prefix=$S2EBUILD/opt \
       --enable-optimized --enable-assertions \
       --target=x86_64 --enable-targets=x86 --enable-jit \
       CC=$(S2EBUILD)/llvm-native/Release/bin/clang \
       CXX=$(S2EBUILD)/llvm-native/Release/bin/clang++

   $ make ENABLE_OPTIMIZED=1 -j4
   $ make ENABLE_OPTIMIZED=0 -j4

   $ # We will refer to these two variables when building the rest of S2E
   $ export CLANG_CXX=$(S2EBUILD)/llvm-native/Release/bin/clang++
   $ export CLANG_CC=$(S2EBUILD)/llvm-native/Release/bin/clang


Building STP
============

STP can only be built inside its source directory. It is recommended to copy
STP source into another directory before building::

   $ cd $S2EBUILD
   $ cp -R $S2ESRC/stp stp
   $ cd stp
   $ bash scripts/configure --with-prefix=$(S2EBUILD)/stp --with-fpic --with-g++=$(CLANG_CXX) --with-gcc=$(CLANG_CC)
   $ make -j4
   $ cp src/c_interface/c_interface.h include/stp


Note that you can use the upstream version of STP as a drop-in replacement
for the STP version that is bundled with S2E.

Building KLEE
=============

::

   $ mkdir $S2EBUILD/klee
   $ cd $S2EBUILD/klee
   $ $S2ESRC/klee/configure \
       --prefix=$(S2EBUILD)/opt \
       --with-llvmsrc=$(S2EBUILD)/llvm-3.0.src \
       --with-llvmobj=$(S2EBUILD)/llvm \
       --with-stp=$(S2EBUILD)/stp \
       --target=x86_64 \
       --enable-exceptions --enable-assertions \
       CC=$(S2EBUILD)/llvm-native/Release/bin/clang \
       CXX=$(S2EBUILD)/llvm-native/Release/bin/clang++

   $ # Compile Release and Debug versions (you can have the both at the same time)
   $ make ENABLE_OPTIMIZED=1 -j4
   $ make ENABLE_OPTIMIZED=0 CXXFLAGS="-g -O0" -j4

Unlike STP, you cannot replace the provided version of KLEE with the upstream one.

Building S2E
============

S2E is based on QEMU and therefore inherits most of its configuration parameters.

::

   $ # Configure and build QEMU in release mode
   $ mkdir $S2EBUILD/qemu-release
   $ cd $S2EBUILD/qemu-release
   $ $S2ESRC/qemu/configure \
       --prefix=$(S2EBUILD)/opt \
       --with-llvm=$(S2EBUILD)/llvm/Release+Asserts  \
       --with-clang=$(S2EBUILD)/llvm-native/Release/bin/clang \
       --with-stp=$(S2EBUILD)/stp \
       --with-klee=$(S2EBUILD)/klee/Release+Asserts \
       --target-list=i386-s2e-softmmu,i386-softmmu \
       --enable-llvm \
       --enable-s2e --compile-all-with-clang

   $ make -j4

   $ # Verify that QEMU works in vanilla mode
   $ ./i386-softmmu/qemu -m 8 # you should see BIOS booting in the VM

   $ # Verify that QEMU works in S2E mode
   $ ./i386-s2e-softmmu/qemu -m 8 # you should see BIOS booting in the VM


For debug mode, proceed as follows.

::

   $ mkdir $S2EBUILD/qemu-debug
   $ cd $S2EBUILD/qemu-debug
   $ $S2ESRC/qemu/configure \
       --prefix=$(S2EBUILD)/opt \
       --with-llvm=$(S2EBUILD)/llvm/Debug+Asserts  \
       --with-clang=$(S2EBUILD)/llvm-native/Release/bin/clang \
       --with-stp=$(S2EBUILD)/stp \
       --with-klee=$(S2EBUILD)/klee/Debug+Asserts \
       --target-list=i386-s2e-softmmu,i386-softmmu \
       --enable-llvm \
       --enable-s2e --compile-all-with-clang

   $ make -j4

Note that you can mix Debug/Release versions of the libraries, depending on what you want to debug.
Using a debug version of KLEE and LLVM may incur >10x slowdowns.


Building S2E Tools
==================

::

   $ cd $S2EBUILD/tools
   $ $S2ESRC/tools/configure \
       --with-llvmsrc=$(S2EBUILD)/$(LLVM_SRC_DIR) \
       --with-llvmobj=$(S2EBUILD)/llvm \
       --with-s2esrc=$(S2ESRC)/qemu \
       --target=x86_64 --enable-assertions \
       CC=$(S2EBUILD)/llvm-native/Release/bin/clang \
       CXX=$(S2EBUILD)/llvm-native/Release/bin/clang++

   $ make -j4


