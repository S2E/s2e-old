==========================
Building the S2E Platform
==========================

The following steps describe the installation process in detail. We assume the installation
is performed on an Ubuntu 10.10 64-bit host system (S2E also works on 64-bit Mac systems).

Build instructions for Windows x64 can be found `here <BuildingS2EWindows.html>`_.

.. contents::

Required Packages
=================

::

    $ sudo apt-get install build-essential
    $ sudo apt-get install subversion
    $ sudo apt-get install git
    $ sudo apt-get install gettext
    $ sudo apt-get install liblua5.1-dev
    $ sudo apt-get install libsdl1.2-dev
    $ sudo apt-get install libsigc++-2.0-dev
    $ sudo apt-get install binutils-dev
    $ sudo apt-get install python-docutils
    $ sudo apt-get install python-pygments
    $ sudo apt-get install nasm

The following commands ask ``apt-get`` to install build dependencies for llvm-2.7
and qemu. We install the build dependencies for llvm-2.7 instead of llvm-3.0
(which is used by S2E) because both of them have almost identical build
dependencies but llvm-3.0 is not available on Ubuntu 10.10::

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
   $ make
   > Go make some coffee, this will take a lot of time

By default, the ``make`` command compiles S2E in release mode. The resulting
binary is placed in ``$S2EDIR/build/qemu-release/i386-s2e-softmmu/qemu``.
To compile in Debug mode, use ``make all-debug``. The makefile automatically
uses the maximum number of available processors in order to speed up compilation.

You can also build each component of S2E manually, as described in `Building
the S2E Framework Manually <BuildingS2EManually.html>`_.

Updating S2E
============

You can use the same make file to recompile S2E either when changing it
yourself or when pulling new versions through ``git``. Note that the makefile
will not automatically reconfigure the packages; for deep changes you might need
to either start from scratch by issuing ``make clean`` or to force
the reconfiguration of specific modules by deleting  the corresponding files from
the ``stamps`` subdirectory.

Rebuilding S2E Documentation
=============================

The S2E documentation is written in `reStructuredText
<http://docutils.sourceforge.net/rst.html>`_ format. For convenience, we also
keep generated HTML files in the repository. Never change HTML files
manually and always recompile them (by invoking ``make`` in the docs folder)
after changing any ``RST`` files.

