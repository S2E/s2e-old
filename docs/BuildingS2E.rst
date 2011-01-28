==========================
Building the S2E Framework
==========================

The following steps describe the installation process in detail. We assume the installation
is performed on an Ubuntu 10.10 64-bit host system (S2E also works on 64-bit
Linux, Mac, and Windows systems).

.. contents::

Required Packages
=================

::

    $ sudo apt-get install build-essential
    $ sudo apt-get install subversion
    $ sudo apt-get install git
    $ sudo apt-get install qemu
    $ sudo apt-get install liblua5.1-dev
    $ sudo apt-get install libsigc++-2.0-dev
    $ sudo apt-get install binutils-dev
    $ sudo apt-get install python-docutils
    $ sudo apt-get install python-pygments
    $ sudo apt-get install nasm

The following commands ask apt-get to install build dependencies for llvm-2.7
and qemu. We install the build dependencies for llvm-2.7 instead of llvm-2.6
(which is used by S2E) because both of them have almost identical build
dependencies but llvm-2.6 is not available on Ubuntu 10.10::

    $ sudo apt-get build-dep llvm-2.7
    $ sudo apt-get build-dep qemu

Checking out S2E
================

S2E source code can be obtained from the S2E GIT repository using the
following commands::

   $ cd $S2EDIR
   $ git clone https://dslabgit.epfl.ch/git/s2e/s2e.git

Alternatively, you can obtain an account on `https://dslabgit.epfl.ch <https://dslabgit.epfl.ch>`_ and
upload your public SSH key to it. If you do not have an SSH key yet,
generate it with the following command::

   $ ssh-keygen -t rsa -C you@email
   > answer questions: store key to default location, optionally set password for it

To upload your key, go to `https://dslabgit.epfl.ch <https://dslabgit.epfl.ch>`_ -> Dashboard -> Manage SSH
keys -> Add SSH key, then copy the content of your ~/.ssh/id_rsa.pub, paste it
into the form, and then press save. Your key is activated within a few moments.
Then, checkout S2E as follows::

   $ cd $S2EDIR
   $ git clone git@dslabgit.epfl.ch:s2e/s2e.git

You can find more information about using git on `http://gitref.org/ <http://gitref.org/>`_ or on
`http://progit.org/ <http://progit.org/>`_.

In order to report bugs, please use https://dslabredmine.epfl.ch. If you would like
to contribute to S2E, please create your own personal clone of S2E on
`https://dslabgit.epfl.ch/s2e/s2e <https://dslabgit.epfl.ch/s2e/s2e>`_, push your changes to it and then send us a
merge request.

Building S2E
============

The recommended method of building S2E is using the S2E Makefile::

   $ mkdir $S2EDIR/build
   $ cd $S2EDIR/build
   $ ln -s ../s2e/Makefile .
   $ make JOBS=4 # Replace 4 with your number of cores
   > Go make some coffee, this will take a lot of time

By default, the ``make`` command compiles S2E in release mode. The resulting
binary is placed in ``$S2EDIR/build/qemu-release/i386-s2e-softmmu/qemu``.
To compile in Debug mode, use ``make all-debug JOBS=4``.

You can also build each component of S2E manually, as described in `Building
the S2E Framework Manually <BuildingS2EManually.html>`_.

Updating S2E
============

You can use the same make file to recompile S2E either when changing it
yourself or when pulling new versions through ``git``. Note that the makefile
will not automatically reconfigure the packages; for deep changes you might need
to either start from scratch by issuing ``make clean`` or to force
the reconfiguration of specific modules by deleting  the corresponding files from
``stamps`` subdirectory.

Rebuilding S2E Documentation
=============================

The S2E documentation is written in `reStructuredText
<http://docutils.sourceforge.net/rst.html>`_ format. For convenience, we also
keep generated HTML files in the repository. Never change HTML files
manually and always recompile them (by invoking ``make`` in the docs folder)
after changing any ``RST`` files.

Preparing a Linux VM Image
==========================

To run S2E, you need a QEMU-compatible virtual machine disk image. S2E can run
any x86 32-bit operating system inside the VM. In the following, we describe how
to install a minimal version of Debian Linux in QEMU::

   $ cd $S2EDIR

   $ # Create an empty disk image
   $ qemu-img create -f qcow2 s2e_disk.qcow2 2G

   $ # Download debian install CD
   $ wget http://cdimage.debian.org/debian-cd/5.0.7/i386/iso-cd/debian-507-i386-businesscard.iso

   $ # Run QEMU and install the OS
   $ qemu s2e_disk.qcow2 -cdrom debian-507-i386-businesscard.iso
   > Follow on-screen instructions to install Debian Linux inside VM
   > Select only "Standard System" component to install

   $ # When you system is installed and rebooted, run the following command
   $ # inside the guest to install C and C++ compilers
   guest$ su -c "apt-get install build-essential"


Compiling the Linux kernel
==========================

Although S2E can run any kernel, it is often convenient to recompile it to suit particular needs.
E.g., enabling Kprobes, adding debug information, etc.
This sections explains how to do it on a Debian system using a ``chroot`` environment.
Using chroot makes it easy to compile a 32-bit kernel package on a 64-bit host.

::

   $ # Install the bootstrapping environment	
   $ sudo apt-get install debootstrap

   $ # Create the directory with the chroot environment
   $ mkdir ~/debian32

   $ # From now on, we need root rights   
   $ sudo -s

   $ # Create the basic chroot environment
   $ debootstrap --arch i386 lenny debian32/ http://mirror.switch.ch/ftp/mirror/debian/
   $ mount -t proc proc debian32/proc

   $ # Activate the chroot
   $ chroot ~/debian32
   
   $ # Setup devices
   $ cd /dev; /sbin/MAKEDEV generic; cd ..

   $ # Install build tools
	$ apt-get install build-essential kernel-package locales

   $ # Set the locale to UTF-8, otherwise perl will complain   
   $ export LANGUAGE=en_US.UTF-8 
   $ export LANG=en_US.UTF-8
   $ export LC_ALL=en_US.UTF-8
   $ locale-gen en_US.UTF-8
   $ dpkg-reconfigure locales

   $ # Download the kernel
   $ mkdir /root/kernel && cd /root/kernel
   $ wget http://www.kernel.org/pub/linux/kernel/v2.6/linux-2.6.26.8.tar.bz2
   $ tar xjvf linux-2.6.26.8.tar.bz2
   $ cd linux-2.6.26.8

   $ # Select your options
   $ make menuconfig

   $ # Compile and generate the packages
   $ make-kpkg --append-to-version=-s2e --rootcmd fakeroot --initrd kernel_image kernel_headers

   
The result of the process is two ``*.deb`` files that you can upload to your VM image.
