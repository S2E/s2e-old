===========================
Supported Debug Information
===========================

Most of the tools can print debug information (e.g., module names, relative program counters, line numbers, etc.) whenever it is available.
Debug information can come from different sources: the execution trace itself, the binary (through the BFD library), or from custom text files.
You need to use the ``-moddir`` command line option to specify the path to the directory that contains the binary modules with
debug information, or text files. You can use ``-moddir`` multiple times if you have multiple directories.

The execution trace contains entries specifying what, where, and when modules are loaded or unloaded. 
The ``ModuleTracer`` plugin records this information whenever an ``Interceptor`` plugin detects module 
loads or unloads. The recorded information is the name, the size, the load address of the module, 
and its native base. 

This information is sufficient to print module names and display module-relative addresses, 
that can be used to look through disassembly from ``objdump`` or IDAPro. 
If the ``ModuleTracer`` plugin is not enabled, no debug information can be retrieved at all, 
since it becomes impossible to know which module the absolute program counters belong to.

The second source of information is the binary itself. As soon as the tools read module information from the trace, 
they attempt to open the corresponding binary, using the recorded name. The paths to the binaries are specified on the 
command line. The S2E tools support any binary that can be parsed by the BFD library.

The third source of information are custom function files. These files describe the binary and list all 
the functions (with their addresses). This file has the same name as the original binary, but suffixed with ".fcn". 
The S2E tools attempt to use it when the original binary cannot be opened.

Below is an example of such a custom file. It can be produced with the ``extractFunctions.py`` script (for IDAPro).
Such files are useful when dealing with Windows binaries that only have PDB debug information, which is not supported for now.

::

  #ImageName rtl8139.sys
  #ImageBase 0x10000
  #ImageSize 0x5200
  0x010300 0x01031f RTFast_EnableInterrupt(x)
  0x010322 0x01033f RTFast_DisableInterrupt(x)
  0x010342 0x0103dc RTFast_Isr(x,x,x)
  0x0103e0 0x01040b RTFast_PacketOK(x)
  0x01040e 0x0104f1 RTFast_IndicatePacket(x)
  0x0104f4 0x0105f7 RTFast_TransferData(x,x,x,x,x,x)
  0x0105fa 0x010664 SyncCardStartXmit0(x)
