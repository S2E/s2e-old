=============================
Preparing VM Images for S2E
=============================

.. contents::

To run S2E, you need a QEMU-compatible virtual machine disk image. S2E can run
any x86 operating system inside the VM.
In this section, we describe how to build a Linux image and present general
requirements and guidelines for various operating systems.


Preparing a Linux VM Image
==========================

In the following, we describe how to install a minimal version of Debian Linux in QEMU.

**Please make sure that you perform all the steps below using QEMU that ships with S2E.**
There will be compatibility problems if you use QEMU that comes with your system (especially
when saving/restoring snapshots).

``$S2EDIR`` refers to the directory where S2E is installed. The paths below assume you
followed the installation tutorials.

::

   $ # Create an empty disk image
   $ $S2EDIR/build/qemu-release/qemu-img create -f raw s2e_disk.raw 2G

   $ # Download debian install CD (the exact version doesn't really matter, below is an example)
   $ wget http://cdimage.debian.org/debian-cd/7.7.0/i386/iso-cd/debian-7.7.0-i386-CD-1.iso

   $ # Run QEMU and install the OS
   $ $S2EDIR/build/qemu-release/i386-softmmu/qemu-system-i386 s2e_disk.raw -m 1024 -cdrom debian-7.7.0-i386-CD-1.iso
   > Follow on-screen instructions to install Debian Linux inside VM (use the most basic setup that includes ssh)
   > If you're having trouble navigating the menus, try specifying a keymap (e.g., -k en-us) 

   $ # When you system is installed and rebooted, run the following command
   $ # inside the guest to install C and C++ compilers
   guest$ su -c "apt-get install build-essential"

You have just set up a disk image in RAW format. You need to convert it to the S2E format for use
with S2E (the reasons for this are described in the next section).

The S2E image format is identical to the RAW format, except that the
image file name has the ".s2e" extension. Therefore, to convert from
RAW to S2E, renaming the file is enough (a symlink is fine too).

::

   $ cp s2e_disk.raw s2e_disk.raw.s2e

The S2E VM Image Format
=======================

Existing image formats are not suitable for multi-path execution, because
they usually mutate internal bookkeeping structures on read operations.
Worse, they may write these mutations back to the disk image file, causing
VM image corruptions. QCOW2 is one example of such formats.

The S2E image format, unlike the other formats, is multi-path aware.
When in S2E mode, writes are local to each state and do not clobber other states.
Moreover, writes are NEVER propagated from the state to the image (or the snapshot). This makes it possible
to share one disk image and snapshots among many instances of S2E.

The S2E image format is identical to the RAW format, except that the
image file name has the ``.s2e`` extension. Therefore, to convert from
RAW to S2E, renaming the file is enough (a symlink is fine too).

The S2E image format stores snapshots in a separate file, suffixed by the name of the
snapshot. For example, if the base image is called "my_image.raw.s2e",
the snapshot ``ready`` (as in ``savevm ready``) will be saved in the file
``my_image.raw.s2e.ready`` in the same folder as ``my_image.raw.s2e``.


General Requirements and Guidelines for VM Images
=================================================

When running in S2E mode, the image **must** be in S2E format. S2E does not support any other image format.

Any x86 image that boots in vanilla QEMU will work in S2E. However, we enumerate a list of tips
and optimizations that will make administration easier and symbolic execution faster.
*These tips should be used as guidelines and are not mandatory.*

Here is a checklist we recommend to follow:


* Install your operating system in the vanilla QEMU. It is the fastest approach. In general, all installation and setup tasks should be done in vanilla QEMU.

* Keep a fresh copy of your OS installation. It is recommended to start with a fresh copy for each analysis task. For instance, if you use an image to test a device driver, avoid using this same image to analyze some spreadsheet component. One image = one analysis. It is easier to manage and your results will be easier to reproduce.

* Once your image (in S2E format) is set up and ready to be run in symbolic execution mode, take a snapshot and resume that snapshot in the S2E-enabled QEMU. This step is not necessary, but it greatly shortens boot times. Booting an image in S2E can take a (very) long time.

* It is recommended to use 128MiB of RAM for the guest OS (or less). S2E is not limited by the amount of memory in any way (it is 64-bit), but your physical machine is.

* Disable fancy desktop themes. Most OSes have a GUI, which consumes resources. Disabling all visual effects will make program analysis faster.

* Disable the screen saver.

* Disable unnecessary services to save memory and speed up the guest. Services like file sharing, printing, wireless network configuration, or firewall are useless unless you want to test them in S2E.

* Avoid the QEMU ``virtio`` network interface for now. In the version of QEMU that is bundled into S2E, there can be random crashes.


Experimental KVM Snapshot Support
=================================

It is possible to boot an image in KVM mode, take a snapshot, and resume
it in the dynamic binary translation (DBT) mode that QEMU normally uses.
This is useful if your guest system is large and avoids cumbersome manipulations to workaround the relative slowness of the DBT
(e.g., starting in QEMU, setting up, converting the disk image to S2E, rebooting again in DBT mode, etc.).

::

    # Set up the guest, take a snapshot
    $ ./qemu-release/x86_64-softmmu/qemu-system-x86_64 -enable-kvm -cpu core2duo -net none

    # Resume the snapshot in DBT mode using vanilla QEMU, to finish the setup
    $ ./qemu-release/x86_64-softmmu/qemu-system-x86_64 -cpu core2duo -net none -loadvm mysnapshot

    # Resume the snapshot in S2E mode
    $ ./qemu-release/x86_64-s2e-softmmu/qemu-system-x86_64 -cpu core2duo -net none -loadvm mysnapshot

Limitations:

- The host CPU in KVM mode must match the virtual CPU in DBT mode. For example, you cannot save a KVM snapshot
  on an Intel CPU and resume it with default settings in DBT mode (i.e., -cpu qemu64, which uses the AMD variations of some instructions).

- The CPUID flags should be matched between KVM and DBT mode. Mismatches do not seem to matter for simple experiments, but may
  lead to guest kernel crashes. You can dump ``/proc/cpuinfo`` in KVM and DBT mode, compare both and add the corresponding tweaks
  to the ``-cpu`` parameter.

- KVM mode does not support S2E custom instructions. They cause an invalid opcode exception in the guest.
  Therefore, you might need to save a second snapshot in DBT mode when using tools such as ``s2eget``.

- It is possible that the guest hangs when resumed in DBT mode from a KVM snapshot.
  Try to save and resume again.

- Resuming DBT snapshots in KVM mode does not seem to work.
