===========================================
How to symbolically execute Linux binaries?
===========================================

.. contents::

In this tutorial, we will show how to symbolically (or concolically) execute *existing* Linux programs,
*without* modifying their source code. In the `Testing a Simple Program <../TestingMinimalProgram.html>`_ tutorial,
we instrumented the source code with S2E instructions to inject symbolic values.
This tutorial shows how to do this directly from the program's command line.

To do so, we use the ``init_env`` shared library and ``LD_PRELOAD``.
This library intercepts the call to the ``main`` function and inserts user-configured symbolic arguments.
This library can also restrict symbolic execution to the program itself or to all the code in the program's address space.


1. Obtaining and compiling ``init_env``
---------------------------------------

The ``init_env`` library can be found in the ``guest`` folder of the S2E
distribution. Copy the entire guest directory to your guest virtual machine, and
run ``make``. This will compile ``init_env`` along with some other useful
tools.


2. Configuring S2E for use with ``init_env``
--------------------------------------------

``init_env`` communicates with several S2E plugins in order to restrict
symbolic execution to the program of interest. The S2E configuration
file must contain default settings for these plugins, as follows:

::

    plugins = {
      -- Enable S2E custom opcodes
      "BaseInstructions",

      -- Track when the guest loads programs
      "RawMonitor",

      -- Detect when execution enters
      -- the program of interest
      "ModuleExecutionDetector",

      -- Restrict symbolic execution to
      -- the programs of interest
      "CodeSelector",
    }

Note that it is not necessary to declare empty configuration blocks
for ``RawMonitor``, ``ModuleExecutionDetector``, or ``CodeSelector``.


3. Using ``init_env``
---------------------

The ``init_env`` library needs to be pre-loaded to your binary using
``LD_PRELOAD``. ``init_env`` intercepts the program's entry point invocation, parses
the command line arguments of the program, configures symbolic execution, and removes ``init_env``-related
parameters, before invoking the origin program's entry point.

For example, the following invokes ``echo`` from GNU CoreUtils, using up to two
symbolic command line arguments, and selecting only code from the ``echo``
binary::

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
    --concolic                     Augments all concrete arguments with symbolic values
    --sym-arg <N>                  Replace by a symbolic argument of length N
    --sym-args <MIN> <MAX> <N>     Replace by at least MIN arguments and at most
                                   MAX arguments, each with maximum length N

Additionally, ``init_env`` will show a usage message if the sole argument given
is ``--help``.

4. Obtaining testcases
----------------------

The command above tests ``echo`` with command line arguments that could take
*any value*. Whenever ``echo`` performs a test on its command line arguments,
S2E checks which cases could be true, and explores all possibilities. Each
possible outcome of the test is explored in it's own *state*. You can see how
S2E switches between these states in S2E's output and the virtual machine
console.

Each state corresponds to a set of command line arguments that cause ``echo`` to
exhibit a particular behavior. Use the ``TestCaseGenerator`` plugin to see the
command line arguments for those states that S2E finished exploring. Add the
following to the ``plugins`` section in ``config.lua``::

    -- Generate a test case when a state is killed
    "ExecutionTracer",
    "TestCaseGenerator",

The ``TestCaseGenerator`` generates output whenever S2E finishes exploring a
state. You need to tell S2E that it can terminate a state once ``echo`` exits.
To do so, call ``s2ecmd kill`` right after calling ``echo``::

    $ LD_PRELOAD=/path/to/guest/init_env/init_env.so /bin/echo \
    --select-process-code --sym-args 0 2 4; \
    /path/to/guest/s2ecmd/s2ecmd kill 0 "echo done"


5. Speeding things up
---------------------

A number of configuration options exist that can make S2E test ``echo`` faster.
These are not enabled by default because results could be confusing. Now that
you're warned, let's add the following to ``kleeArgs`` in ``config.lua``::

    "--flush-tbs-on-state-switch=false",
    "--state-shared-memory=true",

The first option disables flushing the translation block cache when switching
between states. This could lead to problems if you test self-modifying code, or
programs that load libraries dynamically. It should be save for ``echo``.

The second option allows states to share some memory, for example the screen
buffer. This means you will see the output of different states on the screen at
the same time. It should not cause problems for testing programs, unless these
actually read the content of the screen or other shared memory regions.


6. Going further: Symbolic stdin and files
------------------------------------------

You can easily feed symbolic data to your program via ``stdin``.
The idea is to pipe the symbolic output of one program to the input of another.
Symbolic output can be generated using the ``s2ecmd`` utility, located in the
guest tools directory.

::

    $ /path/to/guest/s2ecmd/s2ecmd symbwrite 4 | echo


The command above will pass 4 symbolic bytes to ``echo``.

The easiest way to have your program read symbolic data from *files* (other than
``stdin``) currently involves a ramdisk. You need to redirect the symbolic output
of ``s2ecmd symbwrite`` to a file residing on the ramdisk, then have your program under
test read that file. On many Linux distributions, the ``/tmp`` filesystem resides in
RAM, so using a file in ``/tmp`` works. This can be checked using the ``df``
command: it should print something similar to ``tmpfs 123 456 123 1% /tmp``.
