================================
How to use the init_env library?
================================

.. contents::

The ``init_env`` library simplifies the process of executing programs
symbolically under Linux. It can invoke programs with symbolic command-line
arguments, and can limit symbolic execution to a particular program (using the
`SimpleSelect plugin <../Plugins/SimpleSelect.html>`_).


1. Obtaining and compiling init_env
-----------------------------------

The ``init_env`` library can be found in the ``guest`` folder of the S²E
distribution. Copy the entire guest directory to your guest virtual machine, and
run ``make``. This should compile ``init_env`` along with some other useful
tools.


2. Configuring S²E for use with init_env
----------------------------------------

In order to use code selection with ``init_env``, the SimpleSelect plugin has to
be enabled. Add the following to your ``config.lua``::

    plugins = {
      -- Enable a plugin that handles S2E custom opcodes
      "BaseInstructions",
      
      -- Enable SimpleSelect for use with init_env
      "SimpleSelect",
    }


3. Using init_env
-----------------

The ``init_env`` library needs to be pre-loaded to your binary using
``LD_PRELOAD``. It will then overwrite some C library functions and do its magic
before your program's ``main`` function is called. For example, the following
invokes ``echo`` from GNU CoreUtils, using up to two symbolic command line
arguments, and selecting only code from the ``echo`` binary::

    $ LD_PRELOAD=/path/to/guest/init_env/init_env.so /bin/echo \
    --select-process-code --sym-args 0 2 4

The ``init_env`` library supports the following commands. Each command is added
as a command-line parameter to the program being executed. It is removed before
the program sees the actual command line. In the above example, ``echo`` would
see zero to two command line arguments of up to four bytes, but would not see
the ``--select-process-code`` or ``--sym-args`` argument.

::

    --select-process               Enable forking in the current process only
    --select-process-userspace     Enable forking in userspace-code of the
                                   current process only
    --select-process-code          Enable forking in the code section of the
                                   current binary only
    --sym-arg <N>                  Replace by a symbolic argument with length N
    --sym-arg <N>                  Replace by a symbolic argument with length N
    --sym-args <MIN> <MAX> <N>     Replace by at least MIN arguments and at most
                                   MAX arguments, each with maximum length N

Additionally, ``init_env`` will show a usage message if the sole argument given
is ``--help``.


4. What about other symbolic input?
-----------------------------------

You can easily feed symbolic data to your program on ``stdin``. Create a program
that writes the some symbolic data to stdout, as shown in the example below. You
should try not to create additional forks in this program; that is why it uses
``putchar()`` instead of ``printf()``. This is not an issue if you use
``init_env`` with ``--select-process`` on the next program in the pipe, as this
will automatically unselect the symbolic input generator.

.. code-block:: c

    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>

    #include <s2e.h>

    int main(int argc, char* argv[]) {
        int n_bytes = -1;
        int i;
        
        if (argc != 2) {
            fprintf(stderr, "usage: print_symb n_bytes\n");
            exit(1);
        }

        n_bytes = atoi(argv[1]);
        if (n_bytes < 0) {
            fprintf(stderr, "n_bytes may not be negative");
            exit(1);
        } else if (n_bytes == 0) {
            return 0;
        }

        char* buffer = malloc(n_bytes+1);
        memset(buffer, 0, n_bytes + 1);
        s2e_make_symbolic(buffer, n_bytes, "buffer");

        for (i = 0; i < n_bytes; ++i) {
            putchar(buffer[i]);
        }
        
        return 0;
    }

The easiest way to have your program read symbolic data from *files* (other than
``stdin``) currently involves a ramdisk. You would need to redirect the output
of above program to a file residing on the ramdisk, then have your program under
test read that file. Please search the S²E mailing list for details if you are
interested in this. There are some plans to support symbolic files in
``init_env``, but the feature is not available at the time of writing. 
