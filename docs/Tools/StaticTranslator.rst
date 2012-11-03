=========================================
RevGen - An x86-to-LLVM Static Translator
=========================================

RevGen takes as input a binary executable image (e.g., PE, ELF, etc.) and transforms it to LLVM.

**WARNING:** RevGen is at a very early experimental stage, will most likely not work on
real applications, and comes with no support. The current implementation does not allow to execute the resulting LLVM code and crashes very often.
However, it may be useful to transform short sequences of binary code to LLVM for further analysis.



Building RevGen
===============

RevGen is part of the S2E tools and is automatically built with the rest of S2E.
It is located in ``$S2EDIR/build/tools/Release/bin/static-translator``

RevGen Architecture
===================

RevGen consists of several parts:

1. The translator library

   It is the core part that transforms a sequence of machine instruction bytes to an equivalent
   LLVM representation. It works at a translation block granularity. Given a program counter, the
   library outputs LLVM code up to the next instruction that modifies the control flow.
   The library is located in the ``tools/lib/X86Translator`` folder.


2. The bit code library

   Contains the machine code emulation helpers (essentially ``op_helper.c`` compiled to LLVM).

3. The ``static-translator`` tool itself

   The static translator parses the executable file format, determines where instructions are located,
   and iteratively calls the translator library to get LLVM bitcode.

Using RevGen
============

The static translator works on the Windows AMD PCnet device driver ``pcntpci5.sys`` without crashing.

::

    $ ./static-translator -bitcodelibrary ../lib/X86BitcodeLibrary.bc pcntpci5.sys

The resulting LLVM code is output to ``module.bc``.

Unlike the static translator tool itself, the library is pretty stable, and you can easily reuse it in your own projects.

Potential Improvement Ideas
===========================

Here is a list of things that could be done to improve the static translator:

* More precise disassembly

  RevGen would work perfectly if it was given an accurate list of program counters to disassemble.
  Using tools like Jakstab would help.

* Better code generation

  The generated bitcode is several orders of magnitude larger than necessary (e.g., look at the size difference
  between ``pcntpci5.sys`` and the bitcode file. Implementing LLVM optimization passes aware of the specificities
  of the translator would help. Decompilation techniques would help too.

