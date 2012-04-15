==========================
Compiling the Linux Kernel
==========================

Although S2E can run any Linux kernel, it is often convenient to recompile it to suit particular needs,
e.g., to enable Kprobes, add debug information, etc.

This sections explains how to compile S2E on a Debian system using a ``chroot`` environment.
Using chroot makes it easy to compile a 32-bit kernel package on a 64-bit host.
It is important to compile the guest kernel in 32-bit mode, as S2E does not support 64-bit
guests for now.

::

   $ # Install the bootstrapping environment
   $ sudo apt-get install debootstrap

   $ # Create the directory with the chroot environment
   $ mkdir ~/debian32

   $ # From now on, we need root rights
   $ sudo -s

   $ # Create the basic chroot environment
   $ # Pay attention to --arch i386! It is crucial for correct compilation.
   $ debootstrap --arch i386 lenny debian32/ http://mirror.switch.ch/ftp/mirror/debian/
   $ mount -t proc proc debian32/proc

   $ # Activate the chroot
   $ chroot ~/debian32

   $ # Setup devices
   $ cd /dev; MAKEDEV generic; cd ..

   $ # Set the locale to UTF-8, otherwise perl will complain
   $ export LANGUAGE=en_US.UTF-8
   $ export LANG=en_US.UTF-8
   $ export LC_ALL=en_US.UTF-8
   $ locale-gen en_US.UTF-8
   $ dpkg-reconfigure locales

   $ # Install build tools and developer's libraries for ncurses
   $ apt-get install build-essential kernel-package locales libncurses-dev

   $ # Download the kernel
   $ mkdir /root/kernel && cd /root/kernel
   $ wget http://www.kernel.org/pub/linux/kernel/v2.6/linux-2.6.26.8.tar.bz2
   $ tar xjvf linux-2.6.26.8.tar.bz2
   $ cd linux-2.6.26.8

   $ # Select your options
   $ # On a 64-bit machine, "make menuconfig" automatically switches from 32-bit
   $ # to 64-bit for x86. Thus, precede "make" with "linux32" to ensure the
   $ # selected CPU type is 32-bit. Additionally, select Processor type and
   $ # features ---> Processor family ---> Pentium-Pro
   $ linux32 make menuconfig

   $ # Compile and generate kernel packages (that will be located in ../)
   $ linux32 make-kpkg --append-to-version=-s2e --rootcmd fakeroot --cross-compile - --arch=i386 --initrd kernel_image kernel_headers


The result of the process is two ``*.deb`` files that you can upload to your VM image.
