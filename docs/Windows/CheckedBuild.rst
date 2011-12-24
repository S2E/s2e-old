=======================================
Setting Up the Checked Build of Windows
=======================================

The checked build of Windows is a version that is compiled with more checks and debugging information. 
It catches bugs earlier, before they propagate through the system and cause non-informative blue screens.
*You do not need the checked build for normal S2E use.*

It is not necessary to install the entire checked version of Windows. 
It is sufficient to replace the kernel image and the HAL with a checked version, while leaving the rest as is (i.e., in the "Free Build" state).

The following is an excerpt from an article available `here <http://www.osronline.com/article.cfm?id=405>`_, 
and details the process of replacing various system files with their checked counterparts.

First, download the following file: 

::

   WindowsXP-KB936929-SP3-x86-symbols-full-ENU.exe
     
It contains the checked version of Windows Service Pack 3. You must have the free build of SP3 already up and running.

To install the "partially checked" build, you want just the operating system image and the HAL image. You will need to extract these images from the files you downloaded. For this, run the downloaded executable and take not of the place where it extracts the files. The process will fail with an error message, stating an incompatibility with the currently installed version. Before closing the window, copy the extract files to a new place. Once you close the window, the installer will erase the original files.

Extract the *hal\*.dl_* and *nt\*.dl_* files that are located in *\\i386*, using the *expand* utility. Replace the extension of the files with *.chk*. Copy them to the *system32* folder of the target system.

This gives you ALL the HALs and operating system images. To figure out which version of the files we need to use on our target system, open the following file on your target system:

:: 

%SystemRoot%\repair\setup.log

In this file, you will see which operating system image and HAL were used during the original installation of the system.  For example, you may have:
 
::

\WINDOWS\system32\hal.dll = "halmacpi.dll","2242b"
\WINDOWS\system32\ntkrnlpa.exe = "ntkrpamp.exe","1daa3e"
\WINDOWS\system32\ntoskrnl.exe = "ntkrnlmp.exe","1d3970"

This is telling us that we need *halmacpi.dll*,  *ntkrpamp.exe*, and *ntkrnlmp.exe* for this system. Note that if you do not have *pa* extensions, or if you do not use */PAE* to boot, you would use the version of NT kernel without the *pa* in it. 

Finally, modify *boot.ini*:

::

  multi(0)disk(0)rdisk(0)partition(1)\WINNT="Windows MyOS Checked" /fastdetect /kernel=ntkrnlmp.chk /hal=halacpi.chk


You must replace other system files with their checked versions in **safe mode**. Replacing them in normal mode will trigger the Windows File Protection mechanism, restoring the original file. 


Your S2E configuration file should have the following section:

::

  pluginsConfig.WindowsMonitor = {
    version="XPSP3-CHK",
    userMode=true,
    kernelMode=true,
    monitorModuleLoad=true,
    monitorModuleUnload=true,
    monitorProcessUnload=true
  }

The `WindowsMonitor <../Plugins/WindowsInterceptor/WindowsMonitor.html>`_ plugin has **only** support for the checked version of ntoskrnlpa.exe for now!
