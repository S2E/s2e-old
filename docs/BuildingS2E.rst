==========================
Building the S2E Platform
==========================

The following steps describe the installation process in detail. We assume the 
installation is performed on an Ubuntu 12.04 64-bit host system (S2E also works 
on 64-bit Mac systems, and the build instructions can be found in the last 
chapter).

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

The following commands ask ``apt-get`` to install build dependencies for 
llvm-3.0 and qemu. ::

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

In order to report bugs, please use GitHub's `issue tracker 
<https://github.com/dslab-epfl/s2e/issues>`_. If you would like
to contribute to S2E, please create your own personal clone of S2E on GitHub, 
push your changes to it and then send us a pull request.

You can find more information about using git on `http://gitref.org/ 
<http://gitref.org/>`_ or on
`http://progit.org/ <http://progit.org/>`_.


Building S2E
============

The recommended method of building S2E is using the S2E Makefile::

   $ mkdir $S2EDIR/build
   $ cd $S2EDIR/build
   $ make -f ../s2e/Makefile

   > Go make some coffee, this will take a lot of time

By default, the ``make`` command compiles S2E in release mode. The resulting
binary is placed in 
``$S2EDIR/build/qemu-release/i386-s2e-softmmu/qemu-system-i386``.
To compile in Debug mode, use ``make all-debug``. The Makefile automatically
uses the maximum number of available processors in order to speed up 
compilation.

You can also build each component of S2E manually. Refer to the Makefile for
the commands required to build all individual components.

Updating S2E
============

You can use the same Makefile to recompile S2E either when changing it
yourself or when pulling new versions through ``git``. Note that the Makefile
will not automatically reconfigure the packages; for deep changes you might need
to either start from scratch by issuing ``make clean`` or to force
the reconfiguration of specific modules by deleting  the corresponding files 
from the ``stamps`` subdirectory.

Rebuilding S2E Documentation
=============================

The S2E documentation is written in `reStructuredText
<http://docutils.sourceforge.net/rst.html>`_ format. For convenience, we also
keep generated HTML files in the repository. Never change HTML files
manually and always recompile them (by invoking ``make`` in the docs folder)
after changing any ``RST`` files.


Building the S2E Platform on a Mac 
===================================

Basically, the building process on a Mac follows the same workflow as described 
above. However, since Mac has its own default environment and configurations for 
development, there are still some differences between. 

In the following steps, we assume the installation is performed on a clean Mac 
OS X Mountain Lion 10.8 host system.

Installing Command Line Tools
-----------------------------

The "Command Line Tools for Xcode" provides a toolset for development via 
Terminal on a Mac OS X. There are two alternative ways to install it:

1. Go to the  `Apple Developer Downloads 
   <https://developer.apple.com/downloads>`_ website with an Apple ID (the same 
   as the one for iTunes and App purchases), search for the "command line tools" 
   in the search field on the website, then click the last version of "Command 
   Line Tools (OS X Mountain Lion) for Xcode"(~118MB), download and install it.

2. Download Xcode from either `Apple Developer Downloads 
   <https://developer.apple.com/downloads>`_ or the `Mac App Store 
   <http://itunes.apple.com/us/app/xcode/id497799835?ls=1&mt=12>`_. Xcode is a 
   massive beast (~2GB), make sure you have a high-bandwidth network connection.  
   Once Xcode is installed, launch it, go to Xcode's "Preference" via the menu 
   bar, then find the “Downloads” pane, and download the "Command Line Tools" 
   from within the application.

Installing Homebrew
--------------------

There are several package managers on OS X, such as `Homebrew 
<http://mxcl.github.io/homebrew/>`_ and `MacPorts 
<http://www.macports.org/index.php>`_. In this tutorial, we use Homebrew. To 
install `Homebrew <http://mxcl.github.io/homebrew/>`_, simply run the following 
command in your Terminal:: 

    $ ruby -e "$(curl -fsSL https://raw.github.com/mxcl/homebrew/go)" 

It will prompt you what to do before the installation begins. Once installed, 
insert the Homebrew directory at the top of your ``PATH`` environment variable. You 
can do this by adding the following line at the bottom of your ``~/.profile`` 
file::

    PATH=/usr/local/bin:$PATH
    export PATH
    
Then, run ``brew doctor`` to ensure there are not any potential problems with 
your environment. If you get ::
    
    Your system is ready to brew

you can move on to the next step, otherwise, refer to the Homebrew's 
`Troubleshooting <https://github.com/mxcl/homebrew/wiki/Troubleshooting>`_ to 
fix errors and warnings you might run into.

Installing required packages
----------------------------

After the package manager is ready, type the following commands to install the 
required packages:: 

    $ brew install git wget binutils gettext pkgconfig glib lua libsigc++ nasm

If you want to compile the S2E documentation, you also need ``docutils`` and 
``pygments`` tools, which are both Python packages. Since an out-of-box version 
of Python 2.7 is distributed by Mac OS X, we can directly install the two 
packages once Python's own package manager ``pip`` is installed ::

    $ sudo easy_install pip
    $ sudo pip install docutils pygments

Creating symlinks
-----------------

Unlike the ``apt-get`` of Ubuntu, some libraries installed by Homebrew are not 
placed in the directories that S2E can find. The simplest solution is to create 
symlinks to these libraries in the ``/opt/local/lib/x86_64/`` directory 
respectively.  Maybe you need the following command to create the directory 
first::

    $ mkdir -p /opt/local/lib/x86_64

The symlinks can be created as follows::

    $ sudo ln -s $PATH_TO_ORIGINAL_LIBS $SYMLINK_WITH_SAME_NAME

The ``$PATH_TO_ORIGINAL_LIBS`` variable includes::

    /usr/local/Cellar/gettext/0.18.2/lib/libintl.* 
    /usr/local/Cellar/gettext/0.18.2/lib/libgettextpo.* 
    /usr/local/lib/x86_64/libiberty.a

"*" stands for ``a`` and ``dylib``. 

Getting and Building the S2E source code  
----------------------------------------

Just follow the instructions in the `Checking out S2E`_ and `Building S2E`_ 
sections above.
