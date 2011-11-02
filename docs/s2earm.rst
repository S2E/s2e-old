=======================
S2E for ARM
=======================

The arm-branch of S2E is experimental but here are hints on building and using the ARM port for S2E.  

This tutorial was tested with Ubuntu 10.10 as host system.

Building Instructions
=====================

1. cd buildarm 

2. make all      (or make all-debug)

3.  find binaries in buildarm/qemu-release/arm-s2e-softmmu (S2E enabled) and buildarm/qemu-release/arm-softmmu (S2E not enabled). Replace 'qemu-release' with 'qemu-debug' when running 'make all-debug'.

Hints
======
1. Look at /path/to/s2earm/qemu/tests/s2earm for a test firmware which establishes basic communication with S2E. (look at README for tipps how to compile assembler code).

2. Find the config file in /path/to/config.lua

3. To invoke custom S2E instructions in native ARM applications you can try the provided header: /path/to/s2earm/guest/include/s2earm.h

4. To setup an ARM debian target, try: http://www.aurel32.net/info/debian_arm_qemu.php 

best regards
Andreas Kirchner <akalypse@gmail.com>

                                 +-+-+-+-+-+-+
                                 |S|2|E|A|R|M|
                                 +-+-+-+-+-+-+