=======================================================
Quickly Uploading Programs to the Guest with ``s2eget``
=======================================================

The ``s2eget`` tool allows to easily upload files from the host into the guest in
S2E mode. The typical use case for this tool is to set up a VM snapshot that, when
resumed in S2E mode, automatically downloads a program from the host and starts
symbolically executing it.

.. contents::

Setting up HostFiles Plugin
===========================

To use ``s2eget``, enable the ``HostFiles`` plugin in the S2E configuration file.
Here is a minimal ``config.lua`` to use ``s2eget``:

.. code-block:: lua

   plugins = {
     "BaseInstructions",
     "HostFiles"
   }

   pluginsConfig = {}

   pluginsConfig.HostFiles = {
     baseDirs = {"/path/to/host/dir1", "/path/to/host/dir2"}
   }

The ``pluginsConfig.HostFiles.baseDirs`` configuration option specifies what
directories on the host should be exported to the guest. The paths can be either
absolute, relative, or empty in which case the s2e output directory
will be exported.

Running ``s2eget``
==================

We need to copy the ``s2eget`` binary into the guest image.

1. Boot the VM in the S2E version of QEMU in non-S2E mode::

    host$ $S2EDIR/build/qemu-release/i386-softmmu/qemu-system-i386 s2e_disk.raw.s2e

2. Copy ``s2eget`` into the guest over SSH (or any other method).

3. Launch ``s2eget``, for example, as follows::

    guest$ ./s2eget <filename> && chmod +x ./<filename> && ./<filename>

   where ``<filename>`` specifies what file to download from the host and execute
   in the guest.

   When being run like that in non-S2E mode, ``s2eget`` simply waits.

4. Save a VM snapshot (e.g., call it "ready")

5. Resume the snapshot in S2E mode. Here is an example of how to start in S2E mode::

    host$ $S2EDIR/build/qemu-release/i386-s2e-softmmu/qemu-system-i386 s2e_disk.raw.s2e -s2e-config-file config.lua -loadvm ready

   ``s2eget`` detects that it runs in
   S2E mode and downloads the file. The rest of the command line makes
   the downloaded file executable and then executes it.


The most convenient way of using S2E is to download a bootstrap file
with ``s2eget``, then launch the bootstrap file after resuming in
S2E mode. The bootstrap file can further use ``s2eget`` to download
and execute more files. This way, you can resume the snapshot as
many times as you want, changing the code to run in S2E just by
tweaking the bootstrap file.
