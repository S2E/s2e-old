=======================
S2E for ARM
=======================

Here are hints on building and using the ARM port for S2E.

Hints
======
1. Look at /path/to/s2e/qemu/tests/s2earm for a test firmware which establishes basic communication with S2E. To run the test bios, try:

	/buildpath/qemu-release/arm-s2e-softmmu/qemu-system-arm \
	-s2e-config-file /path/to/config.lua \
	-nographic -kernel /path/to/s2e/qemu/tests/s2earm/hello.bin \
	-s2e-verbose

2. To invoke custom S2E instructions in native ARM applications you can try the provided header: /path/to/s2earm/guest/include/s2earm.h

3. To setup an ARM debian target, download the images here: http://people.debian.org/~aurel32/qemu/arm/ . Then try::
	
	/buildpath/qemu-release/arm-s2e-softmmu/qemu-system-arm \
	-M versatilepb -kernel vmlinuz-2.6.26-2-versatile \
	-initrd initrd.img-2.6.26-2-versatile \
	-hda debian_lenny_arm_standard.qcow2 \
	-append "root=/dev/sda1" \
	-s2e-verbose -s2e-config-file /path/to/config.lua
	

Cross-Compile ARM code
======================
If you want to cross-compile ARM code from a X86 host machine, consider the following hints:

* To compile ARM assembly code on an X86 machine you need to install an ARM toolchain, for example Sourcery G++ Lite for ARM GNU/Linux: http://www.codesourcery.com/sgpp/lite/arm/portal/release1803
* The directory /path/to/s2e/qemu/tests/s2earm contains an ARM port of S2E's testbios. Addional testcases were added to debug S2E's functionality for ARM.
* Build the test firmware (hello.S) with buildHello.sh (this script relies on an ARM toolchain).


Be part of the S2E-ARM community
================================
Feel free to extend the features of S2E for ARM in order to adapt it for your use cases! For example: Port existing S2E-plugins to ARM. In some cases it is possible to make the plugins work for both, X86 and ARM by using qemu's preprocessor macros TARGET_ARM and TARGET_I386.

best regards
Andreas Kirchner <akalypse@gmail.com>

                                 +-+-+-+-+-+-+
                                 |S|2|E|A|R|M|
                                 +-+-+-+-+-+-+