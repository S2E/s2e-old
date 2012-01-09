==============
WindowsMonitor
==============

The WindowsMonitor plugin implements the detection of module and process loads/unloads on the Windows operating system.
It can be referred to as "Interceptor" by other plugins. 
The plugin catches the invocation of specific kernel functions to detect these events.

Options
-------

version=["sp2"|"sp3"]
~~~~~~~~~~~~~~~~~~~~~
Indicates the version of the Windows kernel to monitor. 
These functions have different locations in different versions.
Specifying a wrong version will prevent the plugin from detecting the events.  


userMode=[true|false]
~~~~~~~~~~~~~~~~~~~~~
Specifies whether the plugin should track user-mode events like DLL load and unload.
If you do not analyze user-mode applications, assigning false to this setting will reduce the
amount of instrumentation.

kernelMode=[true|false]
~~~~~~~~~~~~~~~~~~~~~~~
Specifies whether the plugin should track driver load and unload.
If you do not analyze kernel-mode drivers, assigning false to this setting will reduce the
amount of instrumentation.


If not specified, the default value is false.

monitorModuleLoad=[true|false]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
For debugging only. In normal operation must be set to true.

monitorModuleUnload=[true|false]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
For debugging only. In normal operation must be set to true.

monitorProcessUnload=[true|false]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
For debugging only. In normal operation must be set to true.


Configuration Sample
--------------------

::

  pluginsConfig.WindowsMonitor = {
    version="XPSP3",
    userMode=true,
    kernelMode=true,
    monitorModuleLoad=true,
    monitorModuleUnload=true,
    monitorProcessUnload=true
    }

