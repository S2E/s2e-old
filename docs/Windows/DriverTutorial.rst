================================================
Analyzing Windows Drivers: Step-by-Step Tutorial
================================================

In this tutorial, we explain how to symbolically execute the AMD PCnet driver in S2E.
We discuss the preparation of the RAW image in vanilla QEMU, how to write an S2E configuration
file for this purpose, how to launch symbolic execution, and finally how to interpret the results.

.. contents::

Preparing the QEMU image
========================

We want to analyze a PCI device driver, and for this we need an automated way of loading it,
exercising its entry points, then unloading it when we are done.
This can be done manually via the Windows device manager, but can be automated via the <tt>devcon.exe</tt>
utility. You can find this utility on the Internet. <tt>devcon.exe</tt> is a command line program that
allows enumerating device drivers, loading, and unloading them.

::

1. Booting the image
--------------------

First, boot the vanilla QEMU with the following arguments:

   $./i386-softmmu/qemu -fake-pci-name pcnetf -fake-pci-vendor-id 0x1022 -fake-pci-device-id 0x2000 \
    -fake-pci-class-code 2 -fake-pci-revision-id 0x7 -fake-pci-resource-io 0x20 -fake-pci-resource-mem 0x20 \
    -rtc clock=vm -net user -net nic,model=ne2k_pci -hda /home/s2e/vm/windows_pcntpci5.sys.qcow2 

Here is a succing explanation of the command line.

* <tt><b>-fake-pci-name pcnetf</b></tt>: instructs QEMU to enable a fake PCI device called <tt>pcnetf</tt>
which will mimic an AMD PCnet card. <tt>pcnetf</tt> is an arbitrary name that identifies the device in QEMU.
It <em>must</em> be consistent across this tutorial.
Note that you do not need to have a real virtual device for AMD PCnet (even though QEMU has one).
In fact, you can specify any PCI device you want.

* <tt><b>-fake-pci-vendor-id 0x1022 -fake-pci-device-id 0x2000</b></tt>: describe the vendor and device
ID of the fake PCI device. This will trick the plug-and-play module of the guest OS into believing that 
there is a real device installed and will make it load the <tt>pcntpci5.sys</tt> driver.

* <tt><b>-fake-pci-class-code 2 -fake-pci-revision-id 0x7</b></tt>: some additional data that will
populate the PCI device descriptor. This data is device-specific and may be used by the driver.

* <tt><b>-fake-pci-resource-io 0x20</b></tt>: specifies that the device uses 64 bytes of I/O address space.
The base address is assigned by the OS/BIOS at startup.
S2E intercepts all accesses in the assigned I/O range and returns symbolic values upon read. Writes to the range
are discarded.

* <tt><b>-fake-pci-resource-mem 0x20</b></tt>: specifies that the device uses 64 bytes of memory-mapped I/O space.
Same remarks as for <tt>-fake-pci-resource-io</tt>.

* <tt><b>-rtc clock=vm</b></tt>: the real-time clock of QEMU must be set to VM to make symbolic execution work.
<tt style="color: rgb(255,0,0)">TBD: Explain why.</tt>


* <tt><b>-hda /home/s2e/vm/windows_pcntpci5.sys.raw</b></tt>: specifies the path to the disk image.
Note that we use a RAW image here during set up.


* <tt><b>-net user -net nic,model=ne2k_pci</b></tt>: instructs QEMU that we want to use the NE2K virtual NIC adapter.
This NIC adapter is not to be confused with the fake PCI device we set up in previous options.
This NE2K adapter is a real one, and we will use it to upload files to the virtual machine.


2. Copying files
----------------

Copy the <tt>devcon.exe</tt> utility in the Windows image.<br/> 
Then, copy the following script into <tt>c:\s2e\pcnet.bat</tt> (or to any location you wish) in the guest OS.
You may beed to setup and ftp server on your host machine to do the file transfer. The NE2K card we set up previously
should have an address obtained by DHCP. The gateway should be 10.0.2.2. Refer to the QEMU documentation for more details.


   devcon enable @"*VEN_1022&DEV_2000"
   arp -s 192.168.111.1 00-aa-00-62-c6-09
   ping -n 4 -l 999 192.168.111.1
   devcon disable @"*VEN_1022&DEV_2000"


Launch this script to check whether everything is fine. <tt>devcon enable</tt> and <tt>devcon disable</tt> should not produce errors.
Of course, <tt>ping</tt> will fail because the NIC is fake.


3. Setting up the networking configuration
------------------------------------------

1. Before proceeding, <b>reboot</b> the virtual machine.
2. Go to "Network Connections" in the control panel. You should see a disabled (greyed-out) wired network connection corresponding to the fake PCnet card.
Right-click on it, open the properties page, and <b>disable</b> all protocols except TCP/IP.
3. Set the IP address of the fake NIC to 192.168.111.123/24 and the gateway to 192.168.111.1. The actual values do not matter, but you must be consistent with
those in the <tt>pcnet.bat</tt> script.



4. Editing registry settings for PCnet
--------------------------------------

The PCnet driver has a wealth of configration settings. In this section, we will assign bogus values to them. Note that it is important to explicitely set all
settings to something, otherwise Windows will fail the NdisReadConfiguration call in the driver. The NDIS plugin relies on a successful return of that API call
to overwrite the settings with symbolic values.


The registry key containing the settings is the following: <br/>
<tt>HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\{4d36e972-e325-11ce-bfc1-02002be10318}\<b>xxxx</b></tt>, where
<tt><b>xxxx</b></tt> is an integer that can vary from system to system. Select the key that has a value containing "AMD PCNET Family PCI Ethernet Adapter".

The following table lists all the settings that must be set/added.


<table>
<tr><td>Name</td><td>Type</td><td>Value</td></tr>

</table>

