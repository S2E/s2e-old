================================================
Analyzing Windows Drivers: Step-by-Step Tutorial
================================================

In this tutorial, we explain how to symbolically execute the AMD PCnet driver in S2E.
We discuss the preparation of the RAW image in vanilla QEMU, how to write an S2E configuration
file for this purpose, how to launch symbolic execution, and finally how to interpret the results.

Preparing the QEMU image
========================

We want to analyze a PCI device driver, and for this we need an automated way of loading it,
exercising its entry points, then unloading it when we are done.
This can be done manually via the Windows device manager, but can be automated via the *devcon.exe*
utility. You can find this utility on the Internet. *devcon.exe* is a command line program that
allows enumerating device drivers, loading, and unloading them.


1. Booting the image
--------------------

First, boot the vanilla QEMU with the following arguments:

   $./i386-softmmu/qemu -fake-pci-name pcnetf -fake-pci-vendor-id 0x1022 -fake-pci-device-id 0x2000 \
    -fake-pci-class-code 2 -fake-pci-revision-id 0x7 -fake-pci-resource-io 0x20 -fake-pci-resource-mem 0x20 \
    -rtc clock=vm -net user -net nic,model=ne2k_pci -hda /home/s2e/vm/windows_pcntpci5.sys.raw 

Here is an explanation of the command line.

* **-fake-pci-name pcnetf**: instructs QEMU to enable a fake PCI device called *pcnetf* which will mimic an AMD PCnet card. *pcnetf* is an arbitrary name that identifies the device in QEMU. It *must* be consistent across this tutorial. Note that you do not need to have a real virtual device for AMD PCnet (even though QEMU has one). In fact, you can specify any PCI device you want.

* **-fake-pci-vendor-id 0x1022 -fake-pci-device-id 0x2000**: describe the vendor and device ID of the fake PCI device. This will trick the plug-and-play module of the guest OS into believing that  there is a real device installed and will make it load the *pcntpci5.sys* driver.

* **-fake-pci-class-code 2 -fake-pci-revision-id 0x7**: some additional data that will populate the PCI device descriptor. This data is device-specific and may be used by the driver.

* **-fake-pci-resource-io 0x20**: specifies that the device uses 64 bytes of I/O address space. The base address is assigned by the OS/BIOS at startup. S2E intercepts all accesses in the assigned I/O range and returns symbolic values upon read. Writes to the range are discarded.

* **-fake-pci-resource-mem 0x20**: specifies that the device uses 64 bytes of memory-mapped I/O space. Same remarks as for *-fake-pci-resource-io*.

* **-rtc clock=vm**: the real-time clock of QEMU must be set to VM to make symbolic execution work. **TBD: Explain why.**

* **-hda /home/s2e/vm/windows_pcntpci5.sys.raw**: specifies the path to the disk image. Note that we use a RAW image here during set up.

* **-net user -net nic,model=ne2k_pci**: instructs QEMU that we want to use the NE2K virtual NIC adapter. This NIC adapter is not to be confused with the fake PCI device we set up in previous options. This NE2K adapter is a real one, and we will use it to upload files to the virtual machine.


2. Copying files
----------------

Copy the *devcon.exe* utility in the Windows image. 
Then, copy the following script into *c:\s2e\pcnet.bat* (or to any location you wish) in the guest OS.
You may beed to setup and ftp server on your host machine to do the file transfer. The NE2K card we set up previously
should have an address obtained by DHCP. The gateway should be 10.0.2.2. Refer to the QEMU documentation for more details.

::

   devcon enable @"*VEN_1022&DEV_2000"
   arp -s 192.168.111.1 00-aa-00-62-c6-09
   ping -n 4 -l 999 192.168.111.1
   devcon disable @"*VEN_1022&DEV_2000"


Launch this script to check whether everything is fine. *devcon enable* and *devcon disable* should not produce errors.
Of course, *ping* will fail because the NIC is fake.


3. Setting up the networking configuration
------------------------------------------

1. Before proceeding, **reboot** the virtual machine.
2. Go to "Network Connections" in the control panel. You should see a disabled (greyed-out) wired network connection corresponding to the fake PCnet card. Right-click on it, open the properties page, and **disable** all protocols except TCP/IP.
3. Set the IP address of the fake NIC to 192.168.111.123/24 and the gateway to 192.168.111.1. The actual values do not matter, but you must be consistent with those in the *pcnet.bat* script.



4. Editing registry settings for PCnet
--------------------------------------

The PCnet driver has a wealth of configration settings. In this section, we will assign bogus values to them. Note that it is important to explicitely set all
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

5. Converting the image
-----------------------

1. Once you have set registry settings, make sure the adapter is disabled, then shutdown the guest OS.
2. Convert the *RAW* image to *QCOW2* with *qemu-img*.

   ::

       qemu-img convert -O qcow2 /home/s2e/vm/windows_pcntpci5.sys.raw /home/s2e/vm/windows_pcntpci5.sys.qcow2