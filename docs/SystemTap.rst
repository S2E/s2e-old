========================
Using SystemTap with S2E
========================

SystemTap is a powerful tracing framework on Linux. It can intercept any function calls or instructions
in the kernel and invoke a custom scripts. Such scripts have full access to the system state, can leverage
debugging information, etc.

SystemTap provides S2E users a flexible way of controlling symbolic execution.
The user writes a SystemTap scripts with embedded calls to S2E custom instructions.
This allows to inject symbolic values in any place, kill states based on complex
conditions, etc.

In this tutorial, we describe how to build and run SystemTap. We also give several
examples of useful in-vivo analysis that can be achieved. 

.. contents::

Building the Linux kernel
=========================

SystemTap requires a kernel built with the following settings:

- CONFIG_DEBUG_INFO=y
- CONFIG_RELAY=y
- CONFIG_KPROBES=y
- CONFIG_DEBUG_FS=y

Refer to the "Compiling the Linux kernel" section of the `Building S2E tutorial <BuildingS2E.html>`_
for a list of detailed steps.

Install the resulting kernel in the guest OS.

Building SystemTap on the host
==============================

We will compile the SystemTap scripts in the chrooted environment, upload them
in the VM, and run them there. We could also compile the scripts directly inside
the VM, but it is much slower.

In the ``chroot`` environment you use to compile your kernel, do the following:

::

   # Install the compiled kernel, headers, and debug information.
   # You must ensure that kernel-package = 11.015 is installed, later versions (>=12)
   # strip the debug information from the kernel image/modules.   
      
      
   # Adapt all the filenames accordingly 
   $ dpkg -i linux-image-2.6.26.8-s2e.deb
   $ dpkg -i linux-headers-2.6.26.8-s2e.deb   
   
   # Get the SystemTap server, configure, compile, and install
   $ wget wget http://sourceware.org/systemtap/ftp/releases/systemtap-1.3.tar.gz
   $ tar xzvf systemtap-1.3.tar.gz
   $ cd systemtap-1.3
   $ ./configure
   $ make
   $ make install

Building SystemTap on the guest
===============================

Upload the kernel packages in the guest OS, install them, and reboot.
Then, download SystemTap and install it following the same staps as previously described.

Creating a simple S2E script
============================

In this section, we show how to intercept the network packets received by the ``pcnet32`` driver
and replace the content of specific packets with symbolic data.

Create (on the host machine) a ``pcnet32.stp`` file with the following content:

.. code-block:: c

    probe module("pcnet32").function("*") {
        printf("%s -> %s\n", thread_indent(1), probefunc())
    }

    probe module("pcnet32").function("*").return {
        printf("%s <- %s\n", thread_indent(-1), probefunc())
    }


Cross-compile it with SystemTap, adjusting the kernel revision to suite your needs.
Since the chrooted environment runs a different kernel, cross-compiling is mandatory.

::

    $ stap -r 2.6.26.8-s2e -g -m pcnet_probe pcnet32.stp
    WARNING: kernel release/architecture mismatch with host forces last-pass 4.
    pcnet_probe.ko
    
This will result in a module called ``pcnet_probe.ko`` that we will upload to the VM.

Running the script in the guest
===============================

Once you uploaded the ``pcnet_probe.ko`` module in the guest OS, run the following command:

::

    $ staprun pcnet_probe.ko
    
