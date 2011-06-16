==========
RawMonitor
==========

The RawMonitor plugin lets uses specify via a custom instruction whenever a module of interest is loaded or unloaded.
It is useful when using S2E on a new OS for which there is no plugin that automatically extracts this information.
RawMonitor can also be used to analyze raw pieces of code, such as the BIOS, firmware, etc.


Custom Instruction
------------------

Raw monitor defines the following custom instruction (in the ``s2e.h`` header):

.. code-block:: c

   void s2e_rawmon_loadmodule(const char *name, unsigned loadbase, unsigned size);

It takes as parameters the name of the module, where it is loaded in memory, and its size.
Use this function in your code to manually specify module boundaries, like this:

.. code-block:: c

    int main() {
      ...
      s2e_rawmon_loadmodule("myprogram", loadbase, size);
      ...
    }


Options
-------

RawMonitor accepts global options and an arbitrary number of per-module sections.
Per-module options are prefixed with "module." in the documentation. Refer to the
example below for details.

kernelStart=[address]
~~~~~~~~~~~~~~~~~~~~~
Indicates the boundary between the memory mapped in all address spaces
and process-specific memory. On Linux, this value is typically 0xC0000000, while
one Windows it is 0x80000000. Set the value to zero if this distinction
does not make sense (e.g., there are no address spaces).


module.name=["string"]
~~~~~~~~~~~~~~~~~~~~~~~~~~~
The name of the module. This must match the name passed to ``s2e_rawmon_loadmodule``.

module.delay=[true|false]
~~~~~~~~~~~~~~~~~~~~~~~~~
Set to true when the ``s2e_rawmon_loadmodule`` custom instruction is used.
When set to false, RawMonitor considers the module to be loaded when S2E starts. In this
case, it uses the ``module.start`` parameter to determine the runtime address.


module.start=[address]
~~~~~~~~~~~~~~~~~~~~~~
The run-time address of the module. Set to zero if the runtime address is determined
by the custom instruction.

module.size=[integer]
~~~~~~~~~~~~~~~~~~~~~
The size of the module binary.


module.nativebase=[address]
~~~~~~~~~~~~~~~~~~~~~~~~~~~
The default base address of the binary set by the linker.


module.kernel=[true|false]
~~~~~~~~~~~~~~~~~~~~~~~~~~
Whether the module lies above or below the kernel-mode threshold.
Assumes that the module is mapped in all address space at the same location above
the kernel/user-space boundary.



Configuration Sample
--------------------

::

    pluginsConfig.RawMonitor = {
        kernelStart = 0xc0000000,
        myprog_id = {            
            delay = true,
            name = "myprogram",
            start = 0x0,
            size = 52505,
            nativebase = 0x8048000,
            kernelmode = false
        }
    }

