=======================================
Setting Up the Checked Build of Windows
=======================================

The checked build of Windows is a version that is compiled with more checks and debugging information. 
It catches bugs earlier, before they propagate through the system and cause non-informative blue screens.

It is not necessary to install the entire checked version of Windows. 
It is sufficient to replace the kernel image and the HAL with a checked version, while leaving the rest as is (i.e., in the "Free Build" state).

The following is an excerpt from an article available <a href="http://www.osronline.com/article.cfm?id=405">here</a>, 
and details the process of replacing various system files with their checked counterparts.

First, download the following file: <tt>WindowsXP-KB936929-SP3-x86-symbols-full-ENU.exe</tt>. 
It contains the checked version of Windows Service Pack 3. You must have the free build of SP3 already up and running.


To install the "partially checked" build, you want just the operating system image and the HAL image. You will need to extract these images from the files you downloaded. For this, run the downloaded executable and take not of the place where it extracts the files. The process will fail with an error message, stating an incompatibility with the currently installed version. Before closing the window, copy the extract files to a new place. Once you close the window, the installer will erase the original files.

 

Extract the <tt>hal*.dl_</tt> and <tt>nt*.dl_</tt> files that are located in <tt>\i386</tt>, using the <tt>expand</tt> utility. Replace the extension of the files with <tt>.chk</tt>. Copy them to the <tt>system32</tt> folder of the target system.

 


This gives you ALL the HALs and operating system images. To figure out which version of the files we need to use on our target system, open the following file on your target system:

 

<tt>%SystemRoot%\repair\setup.log</tt>

 

In this file, you'll see which operating system image and HAL were used during the original installation of the system.  For example, on my system I found:

 

<ul>
<li>\WINDOWS\system32\hal.dll = "halmacpi.dll","2242b"</li>
<li>\WINDOWS\system32\ntkrnlpa.exe = "ntkrpamp.exe","1daa3e"</li>
<li>\WINDOWS\system32\ntoskrnl.exe = "ntkrnlmp.exe","1d3970"</li>
</ul>
 


This is telling us that we need <tt>halmacpi.dll</tt>,  <tt>ntkrpamp.exe</tt>, and <tt>ntkrnlmp.exe</tt> for this system. Note that if you do not have <tt>pa</tt> extensions, or if you do not use <tt>/PAE</tt> to boot, you would use the version of NT kernel without the <tt>pa</tt> in it. 



Finally, modify <tt>boot.ini</tt>:

 

<tt>multi(0)disk(0)rdisk(0)partition(1)\WINNT="Windows MyOS Checked" /fastdetect /kernel=ntkrnlmp.chk /hal=halacpi.chk</tt></p>

You must replace other system files with their checked versions in <em>safe mode</em>. Replacing them in normal mode will trigger the Windows File Protection mechanism, restoring the original file. 


<i><b>Do not forget to add the checked option to the WindowsMonitor plugin.
</b></i>
<br>
<b style="color: rgb(255, 0, 0);">The WindowsMonitor plugin has only support for the checked version of ntoskrnlpa.exe for now!</b><br>
<br>
Your S2E configuration file should have the following section:

::

pluginsConfig.<a href="../Plugins/WindowsInterceptor/WindowsMonitor.html">WindowsMonitor</a> = {
    version="sp3",
    userMode=true,
    kernelMode=true,
    <b>checked=true</b>,
    monitorModuleLoad=true,
    monitorModuleUnload=true,
    monitorProcessUnload=true
}

