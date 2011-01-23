=============================
Preparing an OS Image for S2E
=============================

.. contents::

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

