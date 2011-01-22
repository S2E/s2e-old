=============================================
Tutorial 1: Testing a Simple Program with S2E
=============================================

The tutorial assumes you already built S2E and prepared VM image as described
on `Building the S2E Framework <BuildingS2E.html>`_ page.

.. contents::

Program to Test
===============

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

We want to explore all possible paths through this program and cover all of its
code.
   
Compiling the Program in the Guest
==================================

Before testing the program in S2E, let us compile and run it in normal QEMU.
Launch QEMU with with the following command::

   $ qemu your_image.qcow2

You need to copy example source code into the VM. As you will likely need this
frequently, we recommend to install either ssh or http server of your host
machine. Then you can copy the code using scp::

   guest$ scp <your_login_on_host>@<your_host_name>:path/to/tutorial1.c .
   guest$ scp <your_login_on_host>@<your_host_name>:path/to/s2e/guest/include/s2e.h .

Compile and try running the example with the following commands::

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
most simple one is to use S2E opcodes library. This library provides a way for
guest code to communicate with S2E system, asking for certain services.

In order to explore all possible paths through the program that correspond to
all possible inputs we want to make that inputs symbolic. To accomplish it we
replace a call to ``fgets()`` by a call to ``s2e_make_symbolic()``:

.. code-block:: c

     ...
     char str[3];
     // printf("Enter two characters: ");
     // if(!fgets(str, sizeof(str), stdin))
     //   return 1;
     s2e_make_symbolic(str, 2, "str");
     str[3] = 0;
     ...

By default S2E will propagate our symbolic values through the program but will
not fork on branches. To enable forking, we should call
``s2e_enable_forking()`` before making symbolic values, and
``s2e_disable_forking()`` after exploring all branches. In addition, as we want
to minimize the amount of code that will execute with forking, we also disable
all interrupts during symbolic execution using
``s2e_disable_all_apic_interrupts()`` and ``s2e_enable_all_apic_interrupts``.

Finally, it would be interesting to see an example of input value that caused a
program to take a particular execution path. For that, we use
``s2e_get_example()`` function that gives a concrete example of symbolic value
that satisfies current path constraints (i.e., all branch conditions along the
execution path).

After these modifications our example program looks like the following:

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

     s2e_disable_all_apic_interrupts();
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

     s2e_enable_all_apic_interrupts();

     return 0;
   }

Compile this program as usual and try running it::

   guest$ gcc -m32 -O3 tutorial1.c -o tutorial1
   guest$ ./tutorial1
   Illegal instruction

You see ``Illegal instruction`` message because all ``s2e_*`` functions use
special CPU instruction that is only recognized by S2E.

Running the Program in S2E
==========================

Now we need to shutdown the VM and reboot it in the S2E, but first we need to
create a simple config file

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

Booting the system in S2E takes a very long time, we use two step process to
speed it up. First, we boot the system in our version of QEMU but with S2E
disabled. Than we save a snapshot and load it in the S2E::

   guest$ su -c halt # shut down qemu
   
   $ $S2EDIR/build/qemu-release/i386-softmmu/qemu your_image.qcow2
   > Wait until Linux is loaded, login into the system. Then press
   > Ctrl + Alt + 2 and type 'savevm 1' then 'quit'.

   $ $S2EDIR/build/qemu-release/i386-s2e-softmmu/qemu your_image.qcow2 -loadvm 1 \
                              -s2e-config-file config.lua -s2e-verbose
   > Wait the snapshot is resumed, then type in the guest
   guest$ ./tutorial1

After you run this command, S2E will start to symbolically execute our example.
We configured S2E to switch states once per second, each time it selects next
state to explore at random. You will see QEMU screen content changing each
second between different possible outputs of our example.

Each state is a completely independent snapshot of the whole system. You can
even interrupt with each state independently, for example by launching
different programs. Try launching ``tutorial1`` in one of the states again!

In the host terminal (i.e., S2E standard output) you will see various
information about state execution, forking and switching. The same output is
also saved into ``s2e-last/messages.txt`` log file. You could try following the
history of one execution state through the log file.

Exploring The Program Faster
============================

In the previous section we made program fork and run along multiple execution
paths.  However, each path continued to run even after the program terminated,
executing operating system code.  This might be nice to visually experience how
S2E works, but in general we want S2E to stop executing each path as soon as
our program terminates.

This is accomplished with ``s2e_kill_state()`` function: it stops executing
current state immediately, and exits S2E if there are no more states to
explore. We should add a call to this function just before our program returns
control to the OS. Before that, we might want to print example values to the
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

     //s2e_enable_all_apic_interrupts();
     s2e_kill_state(0, "program terminated");

     return 0;
   }

Now we should resume our snapshot in QEMU with S2E disabled, edit and recompile
the program, re-save the snapshot and re-load it in S2E::

   $ $S2EDIR/build/qemu-release/i386-softmmu/qemu your_image.qcow2 -loadvm 1
   guest$ edit tutorial1.c
   guest$ gcc -m32 -O3 tutorial1.c -o tutorial1
   > press Ctrl + Alt + 2 and type 'savevm 1' then type 'quit'.

   $ $S2EDIR/build/qemu/i386-s2e-softmmu/qemu your_image.qcow2 -loadvm 1 \
                              -s2e-config-file config.lua -s2e-verbose
   guest$ ./tutorial1

When you run tutorial1 this time, S2E will quickly terminate leaving you with
a log file that you can examine.

Please note that in case your program crashes or exits at some other point
without calling ``s2e_kill_state()``, S2E will not terminate and continue to
execute paths that returned to the system. To avoid that you could write
another program that simply calls ``s2e_kill_state()`` whenever you launch it
and run the tutorial program like this::

   guest$ ./tutorial; ./s2e_kill

