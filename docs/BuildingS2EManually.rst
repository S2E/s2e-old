===================================
Building the S2E Framework Manually
===================================

.. contents::

Before following this guide you should install dependencies as described in
`Building S2E <BuildingS2E.html>`_.

Building LLVM-2.6 With llvm-gcc
===============================

In order to build S2E you must compile LLVM from sources. LLVM installed from
binary distribution packages will not work.

::

   $ S2ESRC=/path/to/s2e
   $ S2EBUILD=/path/to/build

   $ cd $S2EBUILD

   $ # Download and unpack llvm-gcc and llvm-2.6
   $ wget http://llvm.org/releases/2.6/llvm-gcc-4.2-2.6-x86_64-linux.tar.gz
   $ wget http://llvm.org/releases/2.6/llvm-2.6.tar.gz
   $ tar -zxf llvm-gcc-4.2-2.6-x86_64-linux.tar.gz
   $ tar -zxf llvm-2.6.tar.gz

   $ # Create build directory, configure and build
   $ mkdir $S2EBUILD/llvm
   $ cd $S2EBUILD/llvm
   $ ../llvm-2.6/configure \
       --prefix=$S2EBUILD/opt \
       --with-llvmgccdir=$S2EBUILD/llvm-gcc-4.2-2.6-x86_64-linux \
       --enable-optimized
   $ make ENABLE_OPTIMIZED=1 -j4 # Compile release build
   $ make ENABLE_OPTIMIZED=0 -j4 # Compile debug build (you can compile both at the same time)

Building S2E Version of STP
===========================

STP can only be built inside its source directory. It is recommended to copy
STP source into another directory before building::

   $ cd $S2EBUILD
   $ cp -R $S2ESRC/stp stp
   $ cd stp
   $ bash scripts/configure --with-prefix=$(pwd)
   $ make -j4
   $ cp src/c_interface/c_interface.h include/stp


Building the S2E Version of KLEE
================================

::

   $ mkdir $S2EBUILD/klee
   $ cd $S2EBUILD/klee
   $ $S2ESRC/klee/configure \
       --prefix=$S2EBUILD/opt \
       --with-llvmsrc=$S2EBUILD/llvm-2.6 \
       --with-llvmobj=$S2EBUILD/llvm \
       --with-stp=$S2EBUILD/stp \
       --enable-exceptions
   $ make ENABLE_OPTIMIZED=1 -j4 # Compile release build
   $ make ENABLE_OPTIMIZED=0 -j4 # Compile debug build (you can compile both at the same time)


Building S2E (Modified Version of QEMU)
=======================================

::

   $ # Configure and build QEMU in release mode
   $ mkdir $S2EBUILD/qemu-release
   $ cd $S2EBUILD/qemu-release
   $ $S2ESRC/qemu/configure \
       --prefix=$S2EBUILD/opt \
       --with-llvm=$S2EBUILD/llvm/Release \
       --with-llvmgcc=$S2EBUILD/llvm-gcc-4.2-2.6-x86_64-linux/bin/llvm-gcc \
       --with-stp=$S2EBUILD/stp \
       --with-klee=$S2EBUILD/klee/Release \
       --target-list=i386-s2e-softmmu,i386-softmmu \
       --enable-llvm \
       --enable-s2e
   $ make -j4

   $ # Verify that QEMU works in vanilla mode
   $ ./i386-softmmu/qemu -m 8 # you should see BIOS booting in the VM
   $ # Verify that QEMU works in S2E mode
   $ ./i386-s2e-softmmu/qemu -m 8 # you should see BIOS booting in the VM

   $ # Configure and build QEMU in debug mode
   $ mkdir $S2EBUILD/qemu-debug
   $ cd $S2EBUILD/qemu-debug
   $ $S2ESRC/qemu/configure \
       --prefix=$S2EBUILD/opt \
       --with-llvm=$S2EBUILD/llvm/Debug  \
       --with-llvmgcc=$S2EBUILD/llvm-gcc-4.2-2.6-x86_64-linux/bin/llvm-gcc \
       --with-stp=$S2EBUILD/stp \
       --with-klee=$S2EBUILD/klee/Debug \
       --target-list=i386-s2e-softmmu,i386-softmmu \
       --enable-llvm \
       --enable-s2e \
       --enable-debug
   $ make -j4

Building S2E Tools
==================

::

   $ cd $S2EBUILD/tools
   $ $S2ESRC/tools/configure \
       --with-llvmsrc=$S2EBUILD/llvm-2.6 \
       --with-llvmobj=$S2EBUILD/llvm \
       --with-s2esrc=$S2ESRC/qemu
   $ make -j4

