================================================
Analyzing Windows Drivers: Step-by-Step Tutorial
================================================

In this tutorial, we explain how to symbolically execute the AMD PCnet driver in S2E.
We discuss the preparation of the RAW image in vanilla QEMU, how to write an S2E configuration
file for this purpose, how to launch symbolic execution, and finally how to interpret the results.

.. contents::
.. sectnum::

Preparing the QEMU image
========================

We want to analyze a PCI device driver, and for this we need an automated way of loading it,
exercising its entry points, then unloading it when we are done.
This can be done manually via the Windows device manager, but can be automated via the *devcon.exe*
utility. You can find this utility on the Internet. *devcon.exe* is a command line program that
allows enumerating device drivers, loading, and unloading them.

Booting the image
-----------------

First, boot the vanilla QEMU with the following arguments:

::

   $./i386-softmmu/qemu -fake-pci-name pcnetf -fake-pci-vendor-id 0x1022 -fake-pci-device-id 0x2000 \
    -fake-pci-class-code 2 -fake-pci-revision-id 0x7 -fake-pci-resource-io 0x20 -fake-pci-resource-mem 0x20 \
    -rtc clock=vm -net user -net nic,model=ne2k_pci -monitor telnet:localhost:4444,server,nowait \
    -hda /home/s2e/vm/windows_pcntpci5.sys.raw -s

Here is an explanation of the command line.

* **-fake-pci-name pcnetf**: instructs QEMU to enable a fake PCI device called *pcnetf* which will mimic an AMD PCnet card. *pcnetf* is an arbitrary name that identifies the device in QEMU. It *must* be consistent across this tutorial. Note that you do not need to have a real virtual device for AMD PCnet (even though QEMU has one). In fact, you can specify any PCI device you want.

* **-fake-pci-vendor-id 0x1022 -fake-pci-device-id 0x2000**: describe the vendor and device ID of the fake PCI device. This will trick the plug-and-play module of the guest OS into believing that  there is a real device installed and will make it load the *pcntpci5.sys* driver.

* **-fake-pci-class-code 2 -fake-pci-revision-id 0x7**: some additional data that will populate the PCI device descriptor. This data is device-specific and may be used by the driver.

* **-fake-pci-resource-io 0x20**: specifies that the device uses 64 bytes of I/O address space. The base address is assigned by the OS/BIOS at startup. S2E intercepts all accesses in the assigned I/O range and returns symbolic values upon read. Writes to the range are discarded.

* **-fake-pci-resource-mem 0x20**: specifies that the device uses 64 bytes of memory-mapped I/O space. Same remarks as for *-fake-pci-resource-io*.

* **-rtc clock=vm**: the real-time clock of QEMU must be set to VM to make symbolic execution work. **TBD: Explain why.**

* **-hda /home/s2e/vm/windows_pcntpci5.sys.raw**: specifies the path to the disk image. Note that we use a RAW image here during set up.

* **-net user -net nic,model=ne2k_pci**: instructs QEMU that we want to use the NE2K virtual NIC adapter. This NIC adapter is not to be confused with the fake PCI device we set up in previous options. This NE2K adapter is a real one, and we will use it to upload files to the virtual machine.

* **-monitor telnet:localhost:4444,server,nowait**: QEMU will listen on the port 4444 for connections to the monitor. This is useful to take snapshots of the VM.

* **-s**: makes QEMU to listen for incoming GDB connections. We shall see how to make use of this feature later in this tutorial.

Copying files
-------------

Copy the *devcon.exe* utility in the Windows image.
Then, copy the following script into *c:\s2e\pcnet.bat* (or to any location you wish) in the guest OS.
You may beed to setup and ftp server on your host machine to do the file transfer. The NE2K card we set up previously
should have an address obtained by DHCP. The gateway should be 10.0.2.2. Refer to the QEMU documentation for more details.

::

   devcon enable @"*VEN_1022&DEV_2000*"
   arp -s 192.168.111.1 00-aa-00-62-c6-09
   ping -n 4 -l 999 192.168.111.1
   devcon disable @"*VEN_1022&DEV_2000*"


Launch this script to check whether everything is fine. *devcon enable* and *devcon disable* should not produce errors.
Of course, *ping* will fail because the NIC is fake.


Setting up the networking configuration
---------------------------------------

1. Before proceeding, **reboot** the virtual machine.
2. Go to "Network Connections" in the control panel. You should see a disabled (grayed-out) wired network connection corresponding to the fake PCnet card. Right-click on it, open the properties page, and **disable** all protocols except TCP/IP.
3. Set the IP address of the fake NIC to 192.168.111.123/24 and the gateway to 192.168.111.1. The actual values do not matter, but you must be consistent with those in the *pcnet.bat* script.
4. Disable *all* services that generate spurious network traffic (e.g., SSDP, automatic time update, file sharing, etc.). You can use Wireshark to spot these services.
5. Set the ``HKLM\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\ArpRetryCount`` setting of the TCP/IP stack to 0.
   This disables the gratuitous ARP requests mechanism. Since we want to control what packets we send, we do not want Windows to interfere with that.



Editing registry settings for PCnet
-----------------------------------

The PCnet driver has a wealth of configuration settings. In this section, we will assign bogus values to them. Note that it is important to explicitly set all
settings to something, otherwise Windows will fail the *NdisReadConfiguration* call in the driver. The NDIS plugin relies on a successful return of that API call
to overwrite the settings with symbolic values. If the call fails, no symbolic values will be injected, and some paths may be disabled.

The registry key containing the settings is the following:

::

    HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\{4d36e972-e325-11ce-bfc1-02002be10318}\xxxx

where **xxxx** is an integer that can vary from system to system. Select the key that has a value containing "AMD PCNET Family PCI Ethernet Adapter".

The following table lists all the settings that must be set/added.

================     =============   ================
Name                 Type            Value
================     =============   ================
BUS_TO_SCAN          REG_SZ          ALL
BusNumber            REG_SZ          0
BUSTIMER             REG_SZ          0
BusType              REG_SZ          5
EXTPHY               REG_SZ          0
FDUP                 REG_SZ          0
LED0                 REG_SZ          10000
LED1                 REG_SZ          10000
LED2                 REG_SZ          10000
LED3                 REG_SZ          10000
MPMODE               REG_SZ          0
NetworkAddress       REG_SZ          001122334455
Priority8021p        REG_SZ          0
SlotNumber           REG_SZ          0
TcpIpOffload         REG_SZ          0
TP                   REG_SZ          1
================     =============   ================

Converting the image
--------------------

1. Once you have set registry settings, make sure the adapter is disabled, then shutdown the guest OS.
2. Save a copy of the *RAW* image
3. Convert the *RAW* image to *QCOW2* with ``qemu-img``.

   ::

       qemu-img convert -O qcow2 /home/s2e/vm/windows_pcntpci5.sys.raw /home/s2e/vm/windows_pcntpci5.sys.qcow2

Preparing the image for symbolic execution
------------------------------------------

In this step, we will show how to save a snapshot of the guest OS right before it invokes the very first instruction of the driver.
We will use the remote target feature of GDB to connect to the guest OS, set a breakpoint in the kernel, and save a snapshot when a breakpoint is hit.

1. Boot the image using the previous command line. Make sure to use the QCOW2 image, or you will not be able to save snapshots.

   ::

       $./i386-softmmu/qemu -fake-pci-name pcnetf -fake-pci-vendor-id 0x1022 -fake-pci-device-id 0x2000 \\
        -fake-pci-class-code 2 -fake-pci-revision-id 0x7 -fake-pci-resource-io 0x20 -fake-pci-resource-mem 0x20 \\
        -rtc clock=vm -net user -net nic,model=ne2k_pci -monitor telnet:localhost:4444,server,nowait \\
        -hda /home/s2e/vm/windows_pcntpci5.sys.qcow2 -s

2. Once the image is booted, open the command prompt, go to ``c:\s2e`` and type ``pcnet.bat``, **without** hitting enter yet.

3. On the host OS, open a terminal, launch ``telnet``, and save a first snapshot.

   ::

          $ telnet localhost 4444
          Trying 127.0.0.1...
          Connected to localhost.
          Escape character is '^]'.
          QEMU 0.12.2 monitor - type 'help' for more information
          (qemu) savevm ready

   You can use this snapshot to make quick modifications to the VM, without rebooting the guest

4. Now, open GDB, attach to the remote QEMU guest, set a breakpoint in the kernel, then resume execution.
   In this example, we assume that you have installed the **checked build** of Windows XP **SP3** without any update installed.
   If you have a **free build** of Windows XP SP3 (as it comes on the distribution CD), use **0x805A399A** instead of **0x80b3f5d6**.
   This number if the program counter of the call instruction that invokes the entry point of the driver.

   ::

         $ gdb
         (gdb) target remote localhost:1234
         Remote debugging using localhost:1234
         0xfc54dd3e in ?? ()
         (gdb) b *0x80B3F5D6
         Breakpoint 1 at 0x80b3f5d6
         (gdb) c
         Continuing.

5. Return to the guest, and hit ENTER to start executing the ``pcnet.bat`` script.

6. When GDB hits the breakpoint, go to the telnet console, and save the new snapshot under the name **go**.

   ::

         (qemu) savevm go

7. Close QEMU with the ``quit`` command.

8. At this point, you have two snapshots in the ``/home/s2e/vm/windows_pcntpci5.sys.qcow2``:

   a. A snapshot named **ready**, which is in the state right before loading the driver. Use this snapshot to make quick modifications to the guest between runs, if needed.
   b. A snapshot named **go**, which is about to execute the first instruction of the driver.

Configuring S2E
===============

At this point, we have an image ready to be symbolically executed.
In this section, we will explain how to write an S2E configuration file that controls the behavior of the symbolic execution process.
This file specifies what module to symbolically execute, what parts should be symbolically executed, where to inject symbolic values, and how to kill states.

Before proceeding further, create a file called ``pcntpci5.sys.lua``.
S2E uses LUA as an interpreter for configuration files. As such, these files are fully scriptable and can interact with the symbolic execution engine.
In this tutorial, we cover the basic steps of creating such a file.

Configuring KLEE
----------------

The top level section of the configuration file is ``s2e``.
We start by configuring KLEE, using the ``kleeArgs`` subsection.
Refer to the corresponding section of the documentation for more information about each setting.

::

    s2e = {
        kleeArgs = {
            "--use-batching-search",
            "--use-random-path",
            "--use-cex-cache=true",
            "--use-cache=true",
            "--use-fast-cex-solver=true",
            "--max-stp-time=10",
            "--use-expr-simplifier=true",
            "--print-expr-simplifier=false",
            "--flush-tbs-on-state-switch=false",
        }
    }

Specifying the list of plugins
------------------------------

S2E provides the core symbolic execution engine. All the analysis is done by various plugins.
In this step, we will select the plugins required for analyzing Windows device drivers.
Paste the following snippet right after the previous one. In the following parts of the tutorial,
we briefly present each of the plugins.

::

    plugins = {
        "WindowsMonitor",
        "ModuleExecutionDetector",

        "SymbolicHardware",
        "EdgeKiller",
        "Annotation",

        "ExecutionTracer",
        "ModuleTracer",
        "TranslationBlockTracer",

        "FunctionMonitor",
        "StateManager",
        "NdisHandlers"

        "BlueScreenInterceptor",
        "WindowsCrashDumpGenerator",

    }

Selecting the driver to execute
-------------------------------

The ``WindowsMonitor`` plugins monitors Windows events and catches module loads and unloads.
The ``ModuleExecutionDetector`` plugin listens to events exported by ``WindowsMonitor`` and reacts
when it detects specific modules.

Configure ``WindowsMonitor`` as follows:

::

    pluginsConfig = {}

    pluginsConfig.WindowsMonitor = {
        version="sp3",
        userMode=true,
        kernelMode=true,
        checked=false,
        monitorModuleLoad=true,
        monitorModuleUnload=true,
        monitorProcessUnload=true
    }

This configuration assumes that you run the free build version of Windows XP Service Pack 3.

Now, configure ``ModuleExecutionDetector`` as follows to track loads and unloads of ``pcntpci5.sys``.

::

    pluginsConfig.ModuleExecutionDetector = {
        pcntpci5_sys_1 = {
            moduleName = "pcntpci5.sys",
            kernelMode = true,
        },
    }

Configuring symbolic hardware
-----------------------------

The ``SymbolicHardware`` plugin creates fake PCI (or ISA) devices, which are detected by the OS.
All reads from such devices are symbolic and writes are discarded. Symbolic devices can also generate
interrupts and handle DMA.

The following configuration is specific to the AMD PCNet NIC device.

::

    pluginsConfig.SymbolicHardware = {
         pcntpci5f = {
            id="pcnetf",
            type="pci",
            vid=0x1022,
            pid=0x2000,
            classCode=2,
            revisionId=0x7,
            interruptPin=1,
            resources={
                r0 = { isIo=true, size=0x20, isPrefetchatchable=false},
                r1 = { isIo=false, size=0x20, isPrefetchable=false}
            }
        },
    }



Detecting polling loops
-----------------------

Drivers often use polling loops to check the status of registers.
Polling loops cause the number of states to explode. The `EdgeKiller <../Plugins/EdgeKiller.html>`_ plugin relies on the user
to specify the location of each of these loops and kills the states whenever it detects such loops.
Each configuration entry for this plugin takes a pair of addresses specifying an edge in the control flow graph of
the binary. The plugin kills the state whenever it detects the execution of such an edge.

For the ``pcntpci5.sys`` driver, use the following settings:

::

    pluginsConfig.EdgeKiller = {
        pcntpci5_sys_1 = {
            l1 = {0x14040, 0x1401d},
            l2 = {0x139c2, 0x13993},
            l3 = {0x14c84, 0x14c5e}
       }
    }

Annotating driver code
----------------------

S2E comes with a powerful ``Annotation`` plugin that allows users to control the behavior of symbolic execution.
Each annotation comes in the form of a LUA function taking as parameters the current execution state and the instance
of the annotation plugin. Such annotation can be used to inject symbolic values, monitor the execution, trim useless states, etc.

In the following sample, we write an annotation ``init_merge_point1_oncall``
that suspends all the execution states that have reached the end
of the configuration phase of the ``pcntpci5.sys`` driver. The end of the phase is located at the address ``0x169c9``
relative to the native base of the driver. You may want to disassemble the driver for more details.

::

    function init_merge_point1_oncall(state, plg)
            plg:succeed();
    end

    pluginsConfig.Annotation =
    {
        init1 = {
            active=true,
            module="pcntpci5_sys_1",
            address=0x169c9,
            instructionAnnotation="init_merge_point1_oncall"
        }
    }

Tracing driver execution
------------------------

All output is generated by specialized plugins.
S2E does not generate any output by itself, except debugging logs.

In this part of the tutorial, we present three tracing plugins to record module loads/unloads as well as
all executed translation blocks. This can be useful, e.g.,  to generate coverage reports. Analyzing traces is
covered in a different tutorial.

*These plugins have no configurable options. Hence, they do not require configuration sections.*

* The ``ExecutionTracer`` is the main tracing plugin. This plugin abstracts the execution trace file.
  The ``ExecutionTracer`` plugin saves a binary trace file in the ``s2e-last/ExecutionTracer.dat`` file.
  This file is composed of generic trace items. Each item can have an arbitrary format, determined by the various tracing plugins.

* The ``ModuleTracer``  plugin listens to module events exported by the ``WindowsInterceptor`` plugin (or other plugins exporting the ``Interceptor`` interface) and writes them to the trace by invoking API exported by the ``ExecutionTrace`` plugin.

* Finally, the ``TranslationBlockTracer`` plugin writes the register input and output of each executed translation block.
  Whenever a translation block of a module specified in the ``ModuleExecutionDetector`` plugin is executed, the ``TranslationBlockTracer`` plugin records it in the trace.

Specifying consistency models
-----------------------------

S2E enables various consistency models.
The ``NdisHandlers`` plugin implements the **over-approximate**, **local**, **strict**, and **over-constrained** models for
Windows NDIS drivers.
In this tutorial, we show how to set the **strict** model, in which the only symbolic input comes from the hardware.
Feel free to experiment with other models.

The configuration section looks as follows:

::

    pluginsConfig.NdisHandlers = {
        moduleIds = {"pcntpci5_sys_1"},
        hwId = "pcnetf",
        consistency = "strict",
    }

Controlling the execution of entry points
-----------------------------------------

The ``StateManager`` plugins periodically chooses one successful state at random and kills the remaining states.
The ``NdisHandlers`` plugin uses the ``StateManager`` plugin to suspend all paths that successfully returned from the
entry points (e.g., a successful initialization). Whenever no more new translation blocks are covered during a
*timeout* interval, the ``StateManager`` plugin kills all remaining states but one successful, and lets symbolic execution
continue from the remaining state. This copes with the state explosion problem.

::

    pluginsConfig.StateManager = {
        timeout=60
    }

Detecting bugs
--------------

The ``BlueScreenInterceptor`` and ``WindowsCrashDumpGenerator`` turn S2E into a basic bug finder.
The BSOD detector kills all the states that crashes, while the crash dump generator produces dumps that can be opened
and analyzed in WinDbg.

Dump files are as large as the physical memory and take some time to generate, hence the ``BlueScreenInterceptor`` plugin options specify whether to generate a crash dump, and the maximum number of such dumps.

::

    pluginsConfig.BlueScreenInterceptor = {
        generateCrashDump = false,
        maxDumpCount = 2
    }


Running S2E
===========

Now that the configuration file is ready, it is time to launch S2E.
Notice that we use the S2E-enabled QEMU in the **i386-s2e-softmmu** folder.

::

    $./i386-s2e-softmmu/qemu -rtc clock=vm -net user -net nic,model=ne2k_pci -hda pcntpci5.sys.qcow2 -s2e-config-file pcntpci5.sys.lua -loadvm go

This command will create an ``s2e-out-???`` folder, where ``???`` is the sequence number of the run.
``s2e-last`` is a symbolic link that points to the latest run.

The folder contains various files generated by S2E or plugins. Here is a short list:

* **debug.txt**: contains detailed debug output from S2E and all plugins.
* **warnings.txt**: contains warning output from S2E and all plugins.
* **messages.txt**: contains various messages, less verbose than **debug.txt**.
* **s2e.cmdline**: the command line used to launch S2E.
* **s2e.config.lua**: a copy of the configuration file. This is useful if you tweak the configuration file between different runs.
  It allows you to quickly rerun specific experiments, without losing any configuration.
* **s2e.db**: sqlite database, used by some plugins.
* **ExecutionTracer.dat**: the  execution trace generated by the ``ExecutionTracer`` plugin.


