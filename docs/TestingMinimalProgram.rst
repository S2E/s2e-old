=================================
Tutorial 1: Testing Small Program
=================================

The tutorial assumes you already built S2E and prepared VM image as described
on `Building the S2E Framework <BuildingS2E.html>`_ page.

.. contents::

Program to Test
===============

.. code-block:: c

   #include <stdio.h>
   #include <string.h>
   #include <ctype.h>

   int main(int argc, const char** argv)
   {
     if(argc < 2) {
       printf("usage: %s string\n", argv[0]);
       return 1;
     }

     char str[10];
     str[sizeof(str)-1] = 0;
     strncpy(str, argv[1], sizeof(str)-1);

     if(str[0] == 0) {
       printf("String is empty!\n");

     } else {
       if(islower(str[0]))
         printf("First char is lowercase!\n");

       if(isdigit(str[0]))
         printf("First char is digit!\n");

       if(isalnum(str[0]))
         printf("First char is alphanumeric!\n");

       if(str[0] == str[strlen(str)-1])
         printf("First char equals last char!\n");
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

   guest$ scp <your_login_on_host>@<your_host_name>:path/to/example1.c .

Compile and try running the example with the following commands::

   guest$ gcc example1.c -o example
   guest$ ./example abcd
   guest$ ./example 12321


Preparing the Program for S2E
=============================
   
In order to execute the program symbolically, it is necessary to specify what
values should become symbolic. There are many ways to do it in S2E, but the
most simple one is to use S2E opcodes library. This library provides a way for
guest code to communicate with S2E system, asking for certain services.

In order to explore all possible paths through the program that correspond to
all possible inputs (command line argument in this case) we make characters
copied to str symbolic using ``s2e_make_symbolic()`` function:

.. code-block:: c

     ...
     str[sizeof(str)-1] = 0;
     strncpy(str, argv[1], sizeof(str)-1);
     s2e_make_symbolic(str, sizeof(str)-1);
     ...

At the end of the program execution we want to see an example of input that
could lead to this particular execution. For that, we use ``s2e_get_example()``
function to get such example and ``s2e_message()`` function to send it to the
S2E log:

.. code-block:: c

     ...
     s2e_get_example(str, sizeof(str)-1);
     s2e_message(str);

     return 0;
   }

Finally, if the program simply returns to the system, S2E will continue
executing the system in multiple states forever. It is difficult to interract
with such system, so we ask S2E to keep only one (selected at random) state
after finishing the program and discard all others using
``s2e_kill_all_but_one()`` function:

.. code-block:: c

     ...
     s2e_get_example(str, sizeof(str)-1);
     s2e_message(str);

     s2e_kill_all_but_one();

     return 0;
   }

After these modifications the whole program looks like the following:

.. code-block:: c

   #include <stdio.h>
   #include <string.h>
   #include <ctype.h>

   int main(int argc, const char** argv)
   {
     if(argc < 2) {
       printf("usage: %s string\n", argv[0]);
       return 1;
     }

     char str[10];
     str[sizeof(str)-1] = 0;
     strncpy(str, argv[1], sizeof(str)-1);
     s2e_make_symbolic(str, sizeof(str)-1);

     if(str[0] == 0) {
       printf("String is empty!\n");

     } else {
       if(islower(str[0]))
         printf("First char is lowercase!\n");

       if(isdigit(str[0]))
         printf("First char is digit!\n");

       if(isalnum(str[0]))
         printf("First char is alphanumeric!\n");

       if(str[0] == str[strlen(str)-1])
         printf("First char equals last char!\n");
     }

     s2e_get_example(str, sizeof(str)-1);
     s2e_message(str);
     
     s2e_kill_all_but_one();

     return 0;
   }

