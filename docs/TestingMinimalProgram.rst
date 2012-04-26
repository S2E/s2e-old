=================================
Testing a Simple Program with S2E
=================================

This tutorial assumes that you have already built S2E and prepared VM image as described
on the `Building the S2E Framework <BuildingS2E.html>`_ page.

.. contents::

Program to Test
===============

We want to cover all of the code of the following program by exploring all
the possible paths through it.


.. code-block:: c

   #include <stdio.h>
   #include <string.h>

   int main(void)
   {
     char str[3];
     printf("Enter two characters: ");
     if(!fgets(str, sizeof(str), stdin))
       return 1;

     if(str[0] == 0 || str[1] == 0) {
       printf("Not enough characters\n");

     } else {
       if(str[0] >= 'a' && str[0] <= 'z')
         printf("First char is lowercase\n");
       else
         printf("First char is not lowercase\n");

       if(str[0] >= '0' && str[0] <= '9')
         printf("First char is a digit\n");
       else
         printf("First char is not a digit\n");

       if(str[0] == str[1])
         printf("First and second chars are the same\n");
       else
         printf("First and second chars are not the same\n");
     }

     return 0;
   }

   
Compiling the Program in the Guest
==================================

Before testing the program in S2E, compile and run it in the vanilla QEMU
(e.g., that you downloaded on the QEMU's web site).
Launch QEMU with with the following command::

   $ qemu your_image.qcow2

You need to copy the example source code into the VM. As you will likely need to do this
frequently, we recommend to install either ``ssh`` or an http server on your host
machine. Then you can copy the code using ``scp``::

   guest$ scp <your_login_on_host>@<your_host_name>:path/to/tutorial1.c .
   guest$ scp <your_login_on_host>@<your_host_name>:path/to/s2e/guest/include/s2e.h .

Compile and run the example with the following commands::

   guest$ gcc -m32 -O3 tutorial1.c -o tutorial1
   guest$ ./tutorial1
   Enter two characters: ab
   First char is lowercase
   First char is not a digit
   First and second chars are not the same

Preparing the Program for S2E
=============================
   
In order to execute the program symbolically, it is necessary to specify what
values should become symbolic. There are many ways to do it in S2E, but the
simplest one is to use the S2E opcodes library. This library provides a way for
guest code to communicate with the S2E system.

In order to explore all possible paths through the program that correspond to
all possible inputs, we want to make these inputs symbolic. To accomplish this, we
replace the call to ``fgets()`` by a call to ``s2e_make_symbolic()``:

.. code-block:: c

     ...
     char str[3];
     // printf("Enter two characters: ");
     // if(!fgets(str, sizeof(str), stdin))
     //   return 1;
     s2e_make_symbolic(str, 2, "str");
     str[3] = 0;
     ...

By default, S2E propagates the symbolic values through the program but does
not fork on branches. To enable forking, call
``s2e_enable_forking()`` before making symbolic values, and
``s2e_disable_forking()`` after exploring all branches.

Finally, it would be interesting to see an example of input value that cause a
program to take a particular execution path. This can be useful to reproduce a bug
in a debugger, independently of S2E.
For that, use the ``s2e_get_example()`` function. This function gives a concrete example of symbolic values
that satisfy the current path constraints (i.e., all branch conditions along the
execution path).

After these changes, the example program looks as follows:

.. code-block:: c

   #include <stdio.h>
   #include <string.h>
   #include "s2e.h"

   int main(void)
   {
     char str[3];
     // printf("Enter two characters: ");
     // if(!fgets(str, sizeof(str), stdin))
     //   return 1;

     s2e_enable_forking();
     s2e_make_symbolic(str, 2, "str");

     if(str[0] == 0 || str[1] == 0) {
       printf("Not enough characters\n");

     } else {
       if(str[0] >= 'a' && str[0] <= 'z')
         printf("First char is lowercase\n");
       else
         printf("First char is not lowercase\n");

       if(str[0] >= '0' && str[0] <= '9')
         printf("First char is a digit\n");
       else
         printf("First char is not a digit\n");

       if(str[0] == str[1])
         printf("First and second chars are the same\n");
       else
         printf("First and second chars are not the same\n");
     }

     s2e_disable_forking();

     s2e_get_example(str, 2);
     printf("'%c%c' %02x %02x\n", str[0], str[1],
            (unsigned char) str[0], (unsigned char) str[1]);

     return 0;
   }

Compile and run the program as usual::

   guest$ gcc -m32 -O3 tutorial1.c -o tutorial1
   guest$ ./tutorial1
   Illegal instruction

You see the ``Illegal instruction`` message because all ``s2e_*`` functions use
special CPU opcodes that are only recognized by S2E.

Running the Program in S2E
==========================

To run a program in S2E, we have to write a configuration file, then reboot
the system in S2E.

.. code-block:: lua

   -- File: config.lua
   s2e = {
     kleeArgs = {
       -- Run each state for at least 1 second before
       -- switching to the other:
       "--use-batching-search=true", "--batch-time=1.0"
     }
   }
   plugins = {
     -- Enable a plugin that handles S2E custom opcode
     "BaseInstructions"
   }

Booting the system in S2E takes a long time. Use a two-step process to
speed it up. First, boot the system in the version of QEMU that has S2E
disabled. Then, save a snapshot and load it in the S2E::

   guest$ su -c halt # shut down qemu
   
   $ $S2EDIR/build/qemu-release/i386-softmmu/qemu your_image.qcow2
   > Wait until Linux is loaded, login into the system. Then press
   > Ctrl + Alt + 2 and type 'savevm 1' then 'quit'.
   > Notice that we use i386-softmmu, which is the build with S2E DISABLED.

   $ $S2EDIR/build/qemu-release/i386-s2e-softmmu/qemu your_image.qcow2 -loadvm 1 \
                              -s2e-config-file config.lua -s2e-verbose
   > Wait the snapshot is resumed, then type in the guest
   guest$ ./tutorial1
   > Notice that we use i386-s2e-softmmu, which is the build with S2E ENABLED.

After you run this command, S2E starts to symbolically execute the example.
The configuration file instructs S2E to pick a random state once per second.
You will see the QEMU screen content changing every
second for different possible outputs of the example.

Each state is a completely independent snapshot of the whole system. You can
even interact with each state independently, for example by launching
different programs. Try to launch ``tutorial1`` in one of the states again!

In the host terminal (i.e., the S2E standard output), you see various
information about state execution, forking and switching. This output is
also saved into the ``s2e-last/messages.txt`` log file. As an exercise, try to follow the
execution history of a state through the log file.

Exploring the Program Faster
============================

In the previous section, we made the program run along multiple execution
paths.  However, each path continued to run even after the program terminated,
executing operating system code.  This is great to visually experience how
S2E works, but in general we want S2E to stop executing each path as soon as
the program to analyze terminates.

Terminating an execution path is accomplished with the ``s2e_kill_state()`` function.
A call to this function immediately stops executing the
current state and exits S2E if there are no more states to
explore. Add a call to this function just before the program returns
control to the OS. Before that, we might want to print example values in the
S2E log using ``s2e_message()`` or ``s2e_warning()`` functions:

.. code-block:: c

   int main(void)
   {
     char buf[32];
     memset(buf, 0, sizeof(buf));
     ...

     ...
     s2e_get_example(str, 2);
     snprintf(buf, sizeof(buf), "'%c%c' %02x %02x\n", str[0], str[1],
            (unsigned char) str[0], (unsigned char) str[1]);
     s2e_warning(buf);

     s2e_kill_state(0, "program terminated");

     return 0;
   }

Now, resume the snapshot in QEMU with S2E disabled, edit and recompile
the program, re-save the snapshot and re-load it in S2E::

   $ $S2EDIR/build/qemu-release/i386-softmmu/qemu your_image.qcow2 -loadvm 1
   guest$ edit tutorial1.c
   guest$ gcc -m32 -O3 tutorial1.c -o tutorial1
   > press Ctrl + Alt + 2 and type 'savevm 1' then type 'quit'.

   $ $S2EDIR/build/qemu/i386-s2e-softmmu/qemu your_image.qcow2 -loadvm 1 \
                              -s2e-config-file config.lua -s2e-verbose
   guest$ ./tutorial1

Running ``tutorial1`` this time  will make S2E quickly terminate, leaving
a log file that you can examine.

Please note that in case your program crashes or exits at some other point
without calling ``s2e_kill_state()``, S2E will not terminate and will continue to
execute paths that returned to the system. To avoid this, you can write
a program that simply calls ``s2e_kill_state()``. Launch it right after the
invocation to the program that can crash, e.g., as follows:

::

   guest$ ./tutorial; ./s2e_kill

