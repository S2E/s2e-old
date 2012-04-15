=======================
Building S2E on Windows
=======================

**DEPRECATED. This document refers to S2E-QEMU 0.12. Latest versions of S2E were not tested on Windows.**

In this document, we explain how to build S2E on 64-bit versions of Windows.
S2E works on Windows XP x64 and Windows 7.

All the files can be found on the `S2E web site <https://s2e.epfl.ch/projects/s2e/files>`_.


*WARNING: Windows support is currently experimental and may not work/compile on all setups. Only basic symbolic execution
is currently supported. Most of the plugins will not work because of exception support issues.
We recommend to run S2E in a Linux virtual machine (e.g., VMware). This is almost as fast as running S2E natively,
provided that you give a sufficient amount of memory to the virtual machine (at least 4GB).
We welcome contributions for full Windows support!*



Quick install
=============

This consists in 3 steps and takes less than 5 minutes.

1. Install Python 2.6 for Windows and put it on your path.
2. Download the pre-installed archive `s2e-toolchain.zip`
3. Decompress it to some directory. In the following, we assume `c:\\s2e-toolchain`

You will have a complete Unix-like environment, with gdb/gdb-tui.
You can launch the environment by double-clicking on `msys.bat`.


Compiling the toolchain from scratch
====================================

This is more involved and may take 1-2 hours depending on the speed of your machine.

1. Install Python 2.6 for Windows and put it on your path.
2. Download the `s2e-toolchain-archive.zip` file.
3. Decompress it to `c:\\s2e-archive`
4. Decompress `c:\\s2e-archive\\msysCORE-1.0.11-bin.tar.gz` to `c:\\s2e-toolchain`.
5. Launch MSYS by running c:\\s2e-toolchain\\msys.bat
6. Run the following commands:

::

   $ cd /c/s2e-archive
   $ ./setupenv.sh /c/s2e-toolchain

Wait for the build to complete. It will take a long time.


Setting up the environment
==========================

1. Add the following folders to your `%PATH%`, via the control panel:

::

   c:\s2e-toolchain\mingw64\x86_64-w64-mingw32\bin
   c:\s2e-toolchain\mingw64\bin

2. Add the following environment variable (DO NOT put a trailing slash):

::

   C_INCLUDE_PATH=c:\s2e-toolchain\mingw64\x86_64-w64-mingw32\include


Go to the "Compiling S2E" section.


Compiling S2E
=============

*Warning:* Make sure you setup your environment (git and code editors) to use Linux line endings.
Windows line endings may cause the build to fail, especially during the configuration phase.

In the MSYS console, run the following commands:

::

   $ mkdir /c/s2e
   $ git clone https://dslabgit.epfl.ch/git/s2e/s2e.git s2e
   $ make -f s2e/Makefile.win32

There is no x86-64 `llvm-gcc` that produces 64-bit code on Windows.
We use Clang instead, combined with the MINGW's header files. These files are not tweaked
for Clang, hence the large number of warnings you will get. Look at the `Makefile.win32` for details
on how to build each component individually.

Issues
======

`make` tends to deadlock when called with -j2 or higher. If the build seems to
make no progress and `make` uses all the CPU, kill and restart it.

You must copy the `op_helper.bc` file into c:/s2e/i386-s2e-softmmu/ folder (create the folder if necessary).
Make sure you copy the one corresponding to the S2E build you use (release of debug).
