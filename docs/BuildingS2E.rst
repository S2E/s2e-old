==========================
Building the S2E Framework
==========================

The following steps describe installation in detail. We assume the installation
is performed on Ubuntu 10.10 64bit host system (S2E also works on other 64bit
Linux, Mac and Windows systems).

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
$ sudo apt-get build-dep llvm-2.7
$ sudo apt-get build-dep qemu

Checking out S2E
================

S2E source code can be obtained from the DSLab GIT repository. To access it,
you need to obtain an account on https://dslabgit.epfl.ch and upload your ssh
public key to it. If you don't have an ssh key yet, you can generate it with
the following command::

   $ ssh-keygen -t rsa -C you@email
   > answer questions: store key to default location, optionally set password for it

To upload your key, go to https://dslabgit.epfl.ch -> dashboard -> Manage SSH
keys -> Add SSH key, then copy content of your ~/.ssh/id_rsa.pub and paste it
into the form, then press save. In a few moments your key will be ready to use.
Then you can checkout S2E with the following commands::

   $ cd $S2EDIR
   $ git clone git@dslabgit.epfl.ch:s2e/s2e.git

You can find more information about using git on http://gitref.org/ or on
http://progit.org/.

In order to report bugs, please use https://dslabredmine.epfl.ch. If you ever
want to contribute to S2E, please create your own personal clone of S2E on
https://dslabgit.epfl.ch/s2e/s2e, push your changes to it and then send us a
merge request.

Building S2E
============

The recommended method of building S2E is using S2E Makefile::

   $ mkdir $S2EDIR/build
   $ cd $S2EDIR/build
   $ ln -s ../s2e/Makefile .
   $ make JOBS=4 # Replace 4 with your number of cores
   > Go make some coffee, this will take a lot of time

By default, the ``make`` command will compile S2E in release mode. Resulting
binary will be placed in ``$S2EDIR/build/qemu-release/i386-s2e-softmmu/qemu``.
To compile in Debug mode, use ``make all-debug JOBS=4``.

You can also build each component of S2E manually, as described in `Building
The S2E Framework Manually <BuildingS2EManually.html>`_.

Updating S2E
============

You could use the same make file to recompile S2E either when changing it
yourself or when pulling new versions through the git. Note that the makefile
won't automatically reconfigure the packages so for deep changes you might need
to either start from scratch by issuing ``make clean`` or to force
reconfiguration of specific modules by deleting corresponding files from
``stamps`` subdirectory.

Re-Building S2E Documentation
=============================

S2E documentation is written in `reStructuredText
<http://docutils.sourceforge.net/rst.html>`_ format. For convenience, we also
keep generated HTML files in the repository. Please never change HTML files
manualy and always recompile them (by invoking ``make`` in the docs folder)
after changing any RST files.

Preparing Linux VM Image
========================

To run S2E you need a QEMU-compatible virtual machine disk image. S2E can run
any x86 32bit operating system inside the VM. In the following we describe how
to install minimal version of Debian Linux in QEMU::

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

