=============================
Preparing VM Images for S2E
=============================

.. contents::

To run S2E, you need a QEMU-compatible virtual machine disk image. S2E can run
any x86 32-bit operating system inside the VM.
In this section, we describe how to build a Linux image and present general
requirements and guidelines for various operating systems.


Preparing a Linux VM Image
==========================

In the following, we describe how to install a minimal version of Debian Linux in QEMU.
S2E supports only 32-bit systems for now.

::

   $ cd $S2EDIR

   $ # Create an empty disk image
   $ qemu-img create -f raw s2e_disk.raw 2G

   $ # Download debian install CD
   $ wget http://cdimage.debian.org/debian-cd/6.0.2.1/i386/iso-cd/debian-6.0.2.1-i386-businesscard.iso

   $ # Run QEMU and install the OS
   $ qemu s2e_disk.raw -cdrom debian-6.0.2.1-i386-businesscard.iso
   > Follow on-screen instructions to install Debian Linux inside VM
   > Select only "Standard System" component to install

   $ # When you system is installed and rebooted, run the following command
   $ # inside the guest to install C and C++ compilers
   guest$ su -c "apt-get install build-essential"

You have just setup a disk image in RAW format. You need to convert it to QCOW2 for optimal use
with S2E (the reasons for this are described in the next section).

::

   $ qemu-img convert -O qcow2 s2e_disk.raw s2e_disk.qcow2


General Requirements and Guidelines for VM Images
=================================================

There are no specific requirements for the OS image to make it runnable in S2E.
Any x86 32-bit image that boots in vanilla QEMU will work in S2E. However, we enumerate a list of tips
and optimizations that will make administration easier and symbolic execution faster.
*These tips should be used as guidelines and are not mandatory.*

Here is a checklist we recommend to follow:


* Install your operating system in the vanilla QEMU. It is the fastest approach. In general, all installation and setup tasks should be done in vanilla QEMU.

* Always keep a *RAW* image file of your setup. QEMU tends to corrupt *QCOW2* images over time. You can easily convert a RAW image into *QCOW2* using the *qemu-img* tool. Corruptions manifest by weird crashes that did not use to happen before.

* Keep a fresh copy of your OS installation. It is recommended to start with a fresh copy for each analysis task. For instance, if you use an image to test a device driver, avoid using this same image to analyze some spreadsheet component. One image = one analysis. It is easier to manage and your results will be easier to reproduce.

* Once your (QCOW2) image is setup and ready to be run in symbolic execution mode, take a snapshot and resume that snapshot in the S2E-enabled QEMU. This step is not necessary, but it greatly shortens boot times. Booting an image in S2E can take a (very) long time.

* It is recommended to use 128MiB of RAM for the guest OS (or less). S2E is not limited by the amount of memory in any way (it is 64-bit),  but your physical machine is.


The following checklist is specific to Windows guests. All common tips also apply here.



* Disable fancy desktop themes. Windows has a GUI, which consumes resources. Disabling all visual effects will make program analysis faster.
* Disable the screen saver.
* Disable unnecessary services to save memory and speedup the guest. Services like file sharing, printing, wireless network configuration, or firewall are useless unless you want to test them in S2E.

