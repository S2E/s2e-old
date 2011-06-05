=============================
How to Use Execution Tracers?
=============================

.. contents::


Execution tracers are S2E analysis plugins that record various information along the execution of each path.
Here is a list of currently available plugins:

* **ExecutionTracer**: Base plugin upon which all tracers depend. This plugin records fork points so that offline
  analysis tools can reconstruct the execution tree. This plugin is useful by itself to obtain a fork profile
  of the system and answer questions such as: Which branch forks the most? What is causing path explosion?

* **ModuleTracer**: Records when and where the guest OS loads modules, programs, or libraries. Offline analysis tools
  rely on this plugin to display debug information such as which line of code corresponds to which program counter.
  If ModuleTracer is disabled, no debug information will be displayed.

* **TestCaseGenerator**: Outputs a test case whenever a path terminates. The test case consists of concrete input values
  that would exercise the given path.

* **TranslationBlockTracer**: Records information about the executed translation blocks, including the program counter of
  each executed block and the content of registers before and after execution. This plugin is useful to obtain basic block
  coverage.

* **InstructionCounter**: Counts the number of instructions executed on each path in the modules of interest.

Most of the tracers record information only for the configured modules (except ExecutionTracer, which records forks
anywhere in the system). For this, tracers need to know when execution enters and leaves the modules of interest.
Tracers rely on the ModuleExecutionDetector plugin to obtain this information. ModuleExecutionDetector relies itself
on OS monitor plugins to be notified whenever the OS loads or unloads the modules.


Using execution tracers consists of two steps: preparing the binaries for tracing and configuring the S2E plugins.
In the following, we give an end-to-end example of how to obtain the fork profile of an application.

1. Preparing the Binary
=======================

We assume a Linux binary for which source code is available.
To detect module loads on Linux, we use the `RawMonitor <Plugins/RawMonitor.html>`_  plugin. This
plugin requires the user to *manually* specify whenever the modules of interest are loaded. This
can be simply done with a custom instruction. If you use Windows,
the `WindowMonitor <Plugins/WindowsInterceptor/WindowsMonitor.html>`_ plugin handles detection fully automatically.

Here are the steps to prepare the binary:

::

    # Compile your program with debug information
    $ gcc -g -O0 -m32 -o myprogram myprogram.c

    # Obtain the size of the compiled program
    $ ls -l myprogram

    # Obtain the native base address of the binary
    # On Linux, it is something like 0x08048000
    $ objdump / idapro / etc.

    # Add a call to s2e_rawmon_loadmodule (declared in s2e.h) in the main() of your program.
    # This function tells S2E where you program is loaded.
    # You must specify the runtime base address of the program (which is usually the same as the native base address)
    # and the size of the program. You can use the one you obtained previously.

    # If you use an OS monitor plugin that automatically detects module loads (e.g., WindowMonitor),
    # you do not need this step. There is no such plugin yet for Linux.

    # main() {
    #   ...
    #   s2e_rawmon_loadmodule("myprogram", loadbase, size);
    #   ...
    # }

    # Recompile your program
    $ gcc -g -O0 -m32 -o myprogram myprogram.c

2. Configuring Plugins
======================

In the second step, configure the desired plugin. You can use multiple tracers at once. Please refer to the documentation of each
plugin for detailed configuration options.

Your ``config.lua`` file may look as follows. Note that some plugins do not have any option.

::

    s2e = {
        kleeArgs = {
            -- Whatever options you like
        }
    }

    plugins = {
        "BaseInstructions",
        "RawMonitor",
        "ModuleExecutionDetector",
        "ExecutionTracer",
        "ModuleTracer",
        "TranslationBlockTracer"
    }

    pluginsConfig = {}

    pluginsConfig.RawMonitor = {
        kernelStart = 0xc0000000,
        myprog_id = {
            name = "myprogram",
            start = 0x0,
            size = --put your size here, e.g., 52505,
            nativebase = 0x8048000,
            delay = true,
            kernelmode = false
        }
    }

    pluginsConfig.ModuleExecutionDetector = {
        myprog_id = {
            moduleName = "myprogram",
            kernelMode = false
        },
    }

    --Trace all the modules configured in ModuleExecutionDetector
    pluginsConfig.TranslationBlockTracer = {
      manualTrigger = false,
      flushTbCache = true
    }

3. Viewing the traces
=====================

S2E comes with several tools that parse and display the execution traces.
They are located in the `tools`  folder of the source distribution.
You can find the documentation for them on the `main page <index.html>`_.



Mini-FAQ
========

* The RawMonitor plugin complains that it cannot read the program name.
  This is because the string that you passed to ``s2e_rawmon_loadmodule`` is located in an unmapped memory page.
  S2E cannot access unmapped pages. You need to touch that page to let the OS load it before calling ``s2e_rawmon_loadmodule``.

* You followed all steps and no debug information is displayed by the offline tools.

  * Check that you used the name of the modules coherently. The string you specified in ``s2e_rawmon_loadmodule``
    must be identical to what you set in the RawMonitor and ModuleExecutionDetector plugins.
  * Make sure you defined the module to trace in the ModuleExecutionDetector configuration section.
  * Check that your binutils library understands the debug information in the binaries.
  * Make sure you computed the runtime load address properly, especially if you want to analyze a relocatable library.
  * Check the size of your binary
  * Make sure you set the kernel-mode option properly. It must be ``false`` for user-mode programs (more precisely, for
    programs that do not run in the kernel space, above 0x80000000 or 0xC0000000).




