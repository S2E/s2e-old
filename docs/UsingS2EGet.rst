======================================================
Quicky Uploading Programs to the Guest with ``s2eget``
======================================================

The ``s2eget`` tool allows to easily download files from the host into the guest in
S2E mode. The typical use case for this tool is to setup a VM snapshot that, when
resumed in S2E mode, automatically downloads a program from the host and starts
symbolically executing it.

.. contents::

Setting up HostFiles Plugin
===========================

To use ``s2eget``, you enable the ``HostFiles`` plugin in the S2E configuration file.
Add the following lines to your ``config.lua`` file:

.. code-block:: lua

   plugins = {
      ...
      "HostFiles"
   }

   pluginsConfig.HostFiles = {
     baseDirs = {"/path/to/host/dir1", "/path/to/host/dir2"}
   }

The ``pluginsConfig.HostFiles.baseDirs`` configuration option specifies what
directories on the host should be exported to the guest. The paths can be either
absolute, relative, or empty in which case the s2e output directory
will be exported.

Running ``s2eget``
==================

First, boot the VM in the S2E version of QEMU in non-S2E mode. Copy ``s2eget``
into the guest over SSH (or any other method). Then run the tool, for example,
as follows::

  guest$ ./s2eget <filename> && chmod +x ./<filename> && ./<filename>

where ``<filename>`` specifies what file to download from the host and execute
in the guest.

When being run like that in non-S2E mode, ``s2eget`` simply waits. At that
point, save the VM snapshot and then load it in S2E mode. ``s2eget`` will
detect it and download the file. The rest of the command line will make it
executable and execute it.

Note that you could resume this snapshot as many times as you want, changing
the program and/or trying different S2E options.
