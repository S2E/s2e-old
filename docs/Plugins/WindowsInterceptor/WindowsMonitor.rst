==============
WindowsMonitor
==============

The WindowsMonitor plugin implements the detection of module and process loads/unloads on the Windows operating system.
It can be referred to as "Interceptor" by other plugins. <br/>
The plugin catches the invocation of specific kernel functions to detect these events.<br/>

.. contents::

Options
~~~~~~~

<h4>version=["sp2"|"sp3"]</h4>
Indicates the version of the Windows kernel to monitor. <br/>
These functions have different locations in different versions.<br/>
Specifying a wrong version will prevent the plugin from detecting the events.<br/>  


<h4>userMode=[true|false]</h4>
Specifies whether the plugin should track user-mode events like DLL load and unload.
If you do not analyze user-mode applications, assigning false to this setting will reduce the
amount of instrumentation.

<h4>kernelMode=[true|false]</h4>
Specifies whether the plugin should track driver load and unload.
If you do not analyze kernel-mode drivers, assigning false to this setting will reduce the
amount of instrumentation.

<h4>checked=[true|false]</h4>
Specifies whether the VM is running a <a href="../../Windows/CheckedBuild.html">checked version</a> of the Windows <b>kernel</b>.<br/>
<b>The WindowsMonitor plugin has only support for the checked version of ntoskrnlpa.exe for now!</b>
It does not support checked versions of <tt>ntdll.dll</tt>, you must use the free build version of it to monitor user-mode events.
<br>

If not specified, the default value is false.

<h4>monitorModuleLoad=[true|false]</h4>
For debugging only. In normal operation must be set to true.

<h4>monitorModuleUnload=[true|false]</h4>
For debugging only. In normal operation must be set to true.

<h4>monitorProcessUnload=[true|false]</h4>
For debugging only. In normal operation must be set to true.

Configuration Sample
~~~~~~~~~~~~~~~~~~~~

::

  pluginsConfig.WindowsMonitor = {
    version="sp3",
    userMode=true,
    kernelMode=true,
    checked=false,
    monitorModuleLoad=true,
    monitorModuleUnload=true,
    monitorProcessUnload=true
    }

