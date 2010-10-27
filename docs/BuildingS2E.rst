==========================
Building the S2E Framework
==========================

The following steps describe installation in detail. We assume the installation
is performed on Ubuntu 10.10 64bit host system (S2E also works on other 64bit
Linux, Mac and Windows systems).

.. contents::

Required Packages
=================

::

$ sudo apt-get install build-essential
$ sudo apt-get install subversion
$ sudo apt-get install git
$ sudo apt-get install qemu
$ sudo apt-get install liblua5.1-dev
$ sudo apt-get install libsigc++-2.0-dev
$ sudo apt-get install binutils-dev
$ sudo apt-get build-dep llvm-2.7
$ sudo apt-get build-dep qemu

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

Checking out S2E
================

S2E source code can be obtained from the DSLab GIT repository. To access it,
you need to obtain an account on https://dslabgit.epfl.ch and upload your ssh
public key to it. If you don't have an ssh key yet, you can generate it with
the following command::

   $ ssh-keygen -t rsa -C you@email
   > answer questions: store key to default location, optionally set password for it

To upload your key, go to https://dslabgit.epfl.ch -> dashboard -> Manage SSH
keys -> Add SSH key, then copy content of your ~/.ssh/id_rsa.pub and paste it
into the form, then press save. In a few moments your key will be ready to use.
Then you can checkout S2E with the following commands::

   $ cd $S2EDIR
   $ git clone git@dslabgit.epfl.ch:s2e/s2e.git

You can find more information about using git on http://gitref.org/ or on
http://progit.org/.

In order to report bugs, please use https://dslabredmine.epfl.ch. If you ever
want to contribute to S2E, please create your own personal clone of S2E on
https://dslabgit.epfl.ch/s2e/s2e, push your changes to it and then send us a
merge request.

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

Preparing Linux VM Image
========================

To run S2E you need a QEMU-compatible virtual machine disk image. S2E can run
any x86 32bit operating system inside the VM. In the following we describe how
to install minimal version of Debian Linux in QEMU::

   $ cd $S2EDIR

   $ # Create an empty disk image
   $ qemu-img create -f qcow2 s2e_disk.qcow2 2G

   $ # Download debian install CD
   $ wget http://cdimage.debian.org/debian-cd/5.0.6/i386/iso-cd/debian-506-i386-businesscard.iso

   $ # Run QEMU and install the OS
   $ qemu s2e_disk.qcow2 -cdrom debian-506-i386-businesscard.iso
   > Follow on-screen instructions to install Debian Linux inside VM
   > Select only "Standard System" component to install

   $ # When you system is installed and rebooted, run the following command
   $ # inside the guest to install C and C++ compilers
   guest$ su -c "apt-get install build-essential"

