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

For the purpose of this tutorial, also enable the following option:

- CONFIG_PCNET32=y


Refer to the `Building Linux <BuildingLinux.html>`_ tutorial
for a list of detailed steps.

Install the resulting kernel in the guest OS.

Building SystemTap in the ``chroot`` environment of the host
============================================================

We will compile SystemTap and the scripts in the *chrooted* and *32-bit* environment, upload
the scripts in the VM, and run them there. The ``chroot`` environment makes it easy
to compile to 32-bit mode when your host is 64-bit and isolates your production
environment from mistakes.

We could also compile the scripts directly inside
the VM, but it is much slower.

In the ``chroot`` 32-bit environment you use to compile your kernel, do the following:

::

   # Install the compiled kernel, headers, and debug information.
   # You must ensure that kernel-package = 11.015 is installed, later versions (>=12)
   # strip the debug information from the kernel image/modules.
      
      
   # Adapt all the filenames accordingly 
   $ dpkg -i linux-image-2.6.26.8-s2e.deb
   $ dpkg -i linux-headers-2.6.26.8-s2e.deb   
   
   # Get SystemTap, configure, compile, and install.
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

Creating a simple S2E-enabled SystemTap script
==============================================

In this section, we show how to intercept the network packets received by the ``pcnet32`` driver
and replace the content of the IP header field with symbolic values.

Create (on the host machine) a ``pcnet32.stp`` file with the following content:

.. code-block:: c

   # We use the embedded C support of SystemTap to access the S2E
   # custom instructions. A comprehensive set of such instructions can
   # be found in s2e.h. You can adapt them to SystemTap, in case
   # you need them
   
   # Terminate current state.
   # This is a SystemTap function, that can be called from SystemTap code
   function s2e_kill_state(status:long, message: string) %{
      __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x06, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : : "a" ((uint32_t)THIS->status), "b" (THIS->message)
      );
   %}

   # Print message to the S2E log
   # This is a SystemTap function, that can be called from SystemTap code
   function s2e_message(message:string) %{
      __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x10, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : : "a" (THIS->message)
      );
   %}

   # SystemTap also allows to paste arbitrary C code.
   # This is useful when calling other C functions.

   %{
   //Make the specified buffer symbolic, and assign a name to it
   static inline void s2e_make_symbolic(void* buf, int size, const char* name)
   {
      __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x03, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : : "a" (buf), "b" (size), "c" (name)
      );
   }
   %}

   #### Now comes the real stuff ####   
   
   # Take a pointer to the IP header, and make the options length field symbolic.   
   function s2e_inject_symbolic_ip_optionlength(ipheader: long) %{
     uint8_t *data = (uint8_t*)((uintptr_t)(THIS->ipheader + 0));

     uint8_t len;
     s2e_make_symbolic(&len, 1, "ip_headerlength");
     *data = *data & 0xF0;
     *data = *data | ((len) & 0xF);
   %}


   # Instruct SystemTap to intercept the netif_receive_skb kernel function.
   # NIC drivers call this function when they are ready to give the received packet
   # to the kernel.
   probe kernel.function("netif_receive_skb") {
     msg = sprintf("%s: len=%d datalen=%d\n", probefunc(), $skb->len, $skb->data_len)
     s2e_message(msg)
     s2e_inject_symbolic_ip_optionlength($skb->data)
   }

   
   # Instruct SystemTap to intercept the pcnet32_start_xmit in the pcnet32 driver.
   # We also tell S2E to kill the current state.
   # Intercepting this function can be useful to analyze the reaction of the kernel
   # to the reception of a (symbolic) packet.
   probe module("pcnet32").function("pcnet32_start_xmit") {
     msg = sprintf("%s: len=%d datalen=%d\n", probefunc(), $skb->len, $skb->data_len)
     s2e_message(msg)
     s2e_kill_state(0, "pcnet32_start_xmit")
   }


Compile the script with SystemTap in the ``chroot`` environment, adjusting the kernel revision to suite your needs.

::

    $ stap -r 2.6.26.8-s2e -g -m pcnet_probe pcnet32.stp
    WARNING: kernel release/architecture mismatch with host forces last-pass 4.
    pcnet_probe.ko
    
This will result in a module called ``pcnet_probe.ko`` that we will upload to the VM.
Refer to `how to prepare an OS image <ImageInstallation.html>`_ to learn how to do
it efficiently.

Running the script in S2E
=========================

Create the ``tcpip.lua`` configuration file with the following content:

::

   s2e = {
     kleeArgs = {
        "--use-batching-search",
        "--use-random-path",
     }
   }


   plugins = {
     --This is required for s2e_make_symbolic
     "BaseInstructions",
   }

   pluginsConfig = {}

  

Start S2E with port forwarding enabled by adding ``-redir tcp:2222::22 -redir udp:2222::22``
to the QEMU command line. This will redirect ports 2222 from ``localhost`` to the guest
port 22. Adapt the name of the disk image to suite your needs.

::

   $ qemu -rtc clock=vm -net user -net nic,model=pcnet -redir tcp:2222::22 -redir udp:2222::22 \
       -hda linux_tcpip.qcow2 -s2e-config-file tcpip.lua -loadvm ready

Once you uploaded the ``pcnet_probe.ko`` module in the guest OS, run the following command in the guest:

::

    $ staprun pcnet_probe.ko
    
This will load the probe into the kernel. Symbolic execution will start when the network card
receives the first packet. To send a packet, open a console in the guest, and use ``netcat``
to send a UDP packet:

::

   $ nc -u localhost 2222
   
Type some characters, and press enter.

