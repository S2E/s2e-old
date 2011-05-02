====================================
Building the S2E Platform on Windows
====================================

In this document, we explain how to build S2E for Windows XP x64.
Windows Vista and 7 may work but were not tested. 

All the files can be found on the `S2E web site <https://s2e.epfl.ch/projects/s2e/files>`_.

Installing the Toolchain the Fast Way
=====================================

This consists in 3 steps and takes less than 5 minutes.

1. Install Python 2.6 for Windows and put it on your path.
2. Download the pre-installed archive `s2e-toolchain.zip`
3. Decompress it to some directory. In the following, we assume `c:\\s2e-toolchain`

You will have a complete Unix-like environment, with gdb/gdb-tui.
You can launch the environment by double-clicking on `msys.bat`.


Installing the Toolchain the Slow Way
=====================================

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


Setting up the Environment
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
