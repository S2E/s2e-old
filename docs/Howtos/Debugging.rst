====================
Debugging Guest Code
====================

It is possible to attach GDB to any running instance of S2E. S2E relies on the QEMU's GDB interface, which can
be enabled with the `-s` command line option. This option creates a socket on the port number 1234.

::

   $ ./i386-s2e-softmmu/qemu  -s2e-config-file config.lua -s

Once the guest is launched and the program is running, attach GDB to it.

::

   $ gdb /path/to/my/prog
   (gdb) target remote localhost:1234
   #use gdb as usual (set breakpoints, source directories, single-step, etc.).

Remarks
========

* GDB can only manipulate the current path. Use the DFS search strategy to have a coherent debugging experience.
* GDB cannot inspect symbolic variables. If you attempt to display a symbolic variable, S2E will concretize it.
* You can also debug kernel-mode code.

Useful tips
===========

* At any point, if you feel that symbolic execution got stuck, attach GDB to the running S2E instance to check
  what code is being executed.
