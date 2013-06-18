==========================
Compiling the Linux Kernel
==========================

Although S2E can run any Linux kernel, it is often convenient to recompile it to suit particular needs,
e.g., to enable Kprobes, add debug information, etc.

This sections explains how to compile S2E on a Debian system using a ``chroot`` environment.

::

   $ # Install the bootstrapping environment
   $ sudo apt-get install debootstrap

   $ # Create the directory with the chroot environment
   $ mkdir ~/debian

   $ # From now on, we need root rights
   $ sudo -s

   $ # Create the basic chroot environment
   $ debootstrap lenny debian/ http://mirror.switch.ch/ftp/mirror/debian/
   $ mount -t proc proc debian/proc

   $ # Activate the chroot
   $ chroot ~/debian

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
   $ make menuconfig

   $ # Compile and generate kernel packages (that will be located in ../)
   $ make-kpkg --append-to-version=-s2e --rootcmd fakeroot --cross-compile --initrd kernel_image kernel_headers


The result of the process is two ``*.deb`` files that you can upload to your VM image.
