===================================
Building the S2E Framework Manually
===================================

.. contents::

Building LLVM-2.6 With llvm-gcc
===============================

In order to build S2E you must compile LLVM from sources. LLVM installed from
binary distribution packages will not work.

::

   $ S2EDIR=/path/to/s2e
   $ cd $S2EDIR

   $ # Download and unpack llvm-gcc and llvm-2.6
   $ wget http://llvm.org/releases/2.6/llvm-gcc-4.2-2.6-x86_64-linux.tar.gz
   $ wget http://llvm.org/releases/2.6/llvm-2.6.tar.gz
   $ tar -zxf llvm-gcc-4.2-2.6-x86_64-linux.tar.gz
   $ tar -zxf llvm-2.6.tar.gz

   $ # Create build directory, configure and build
   $ mkdir llvm-build
   $ cd llvm-build
   $ ../llvm-2.6/configure \
       --prefix=$S2EDIR/opt \
       --with-llvmgccdir=$S2EDIR/llvm-gcc-4.2-2.6-x86_64-linux \
       --enable-optimized
   $ make ENABLE_OPTIMIZED=1 -j4 # Compile release build
   $ make ENABLE_OPTIMIZED=0 -j4 # Compile debug build (you can compile both at the same time)

Building S2E Version of STP
===========================

STP can only be built inside its source directory. It is recommended to copy
STP source into another directory before building::

   $ cd $S2EDIR
   $ cp -R s2e/stp stp-build
   $ cd stp-build
   $ bash scripts/configure --with-prefix=$(pwd)
   $ make
   $ cp src/c_interface/c_interface.h include/stp


Building the S2E Version of KLEE
================================

::

   $ cd $S2EDIR
   $ mkdir klee-build
   $ cd klee-build
   $ ../s2e/klee/configure \
       --prefix=$S2EDIR/opt \
       --with-llvmsrc=$S2EDIR/llvm-2.6 \
       --with-llvmobj=$S2EDIR/llvm-build \
       --with-stp=$S2EDIR/stp-build \
       --enable-exceptions
   $ make ENABLE_OPTIMIZED=1 -j4 # Compile release build
   $ make ENABLE_OPTIMIZED=0 -j4 # Compile debug build (you can compile both at the same time)


Building S2E (Modified Version of QEMU)
=======================================

::

   $ cd $S2EDIR

   $ # Configure and build QEMU in release mode
   $ mkdir qemu-build-release
   $ cd qemu-build-release
   $ ../s2e/qemu/configure \
       --prefix=$S2EDIR/opt \
       --with-llvm=$S2EDIR/llvm-build/Release \
       --with-llvmgcc=$S2EDIR/llvm-gcc-4.2-2.6-x86_64-linux/bin/llvm-gcc \
       --with-stp=$S2EDIR/stp-build \
       --with-klee=$S2EDIR/klee-build/Release \
       --target-list=i386-s2e-softmmu,i386-softmmu \
       --enable-llvm \
       --enable-s2e
   $ make -j4

   $ # Verify that QEMU works in vanilla mode
   $ ./i386-softmmu/qemu -m 8 # you should see BIOS booting in the VM
   $ # Verify that QEMU works in S2E mode
   $ ./i386-s2e-softmmu/qemu -m 8 # you should see BIOS booting in the VM

   $ # Configure and build QEMU in debug mode
   $ mkdir qemu-build-debug
   $ cd qemu-build-debug
   $ ../s2e/qemu/configure \
       --prefix=$S2EDIR/opt \
       --with-llvm=$S2EDIR/llvm-build/Debug  \
       --with-llvmgcc=$S2EDIR/llvm-gcc-4.2-2.6-x86_64-linux/bin/llvm-gcc \
       --with-stp=$S2EDIR/stp-build \
       --with-klee=$S2EDIR/klee-build/Debug \
       --target-list=i386-s2e-softmmu,i386-softmmu \
       --enable-llvm \
       --enable-s2e \
       --enable-debug
   $ make -j4

Building S2E Tools
==================

::

   $ cd $S2EDIR
   $ mkdir tools-build
   $ cd tools-build
   $ ../s2e/tools/configure \
       --with-llvmsrc=$S2EDIR/llvm-2.6 \
       --with-llvmobj=$S2EDIR/llvm-build \
       --with-s2esrc=$S2EDIR/s2e/qemu \
       --with-s2eobj=$S2EDIR/qemu-build-release
   $ make -j4

