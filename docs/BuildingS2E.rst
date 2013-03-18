==========================
Building the S2E Platform
==========================

The following steps describe the installation process in detail. We assume the installation
is performed on an Ubuntu 12.04 64-bit host system (S2E also works on 64-bit Mac systems).

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

The following commands ask ``apt-get`` to install build dependencies for llvm-3.0
and qemu. ::

    $ sudo apt-get build-dep llvm-3.0
    $ sudo apt-get build-dep qemu

Checking out S2E
================

S2E source code can be obtained from the S2E GIT repository using the
following commands::

   $ cd $S2EDIR
   $ git clone https://github.com/dslab-epfl/s2e.git

This will clone the S2E repository into ``$S2EDIR/s2e``.

You can also clone S2E via SSH::

   $ cd $S2EDIR
   $ git clone git@github.com:dslab-epfl/s2e.git

In order to report bugs, please use GitHub's `issue tracker <https://github.com/dslab-epfl/s2e/issues>`_. If you would like
to contribute to S2E, please create your own personal clone of S2E on GitHub, push your changes to it and then send us a
pull request.

You can find more information about using git on `http://gitref.org/ <http://gitref.org/>`_ or on
`http://progit.org/ <http://progit.org/>`_.


Building S2E
============

The recommended method of building S2E is using the S2E Makefile::

   $ mkdir $S2EDIR/build
   $ cd $S2EDIR/build
   $ make -f ../s2e/Makefile

   > Go make some coffee, this will take a lot of time

By default, the ``make`` command compiles S2E in release mode. The resulting
binary is placed in ``$S2EDIR/build/qemu-release/i386-s2e-softmmu/qemu-system-i386``.
To compile in Debug mode, use ``make all-debug``. The Makefile automatically
uses the maximum number of available processors in order to speed up compilation.

You can also build each component of S2E manually. Refer to the Makefile for
the commands required to build all inidividual components.

Updating S2E
============

You can use the same Makefile to recompile S2E either when changing it
yourself or when pulling new versions through ``git``. Note that the Makefile
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

