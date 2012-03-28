/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#ifndef _NT_DDK_H_

#define _NT_DDK_H_

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>

namespace s2e {
namespace windows {

struct MDL32 {
    uint32_t Next; //struct _MDL *
    CSHORT Size;
    CSHORT MdlFlags;
    uint32_t Process; //struct _EPROCESS *
    uint32_t MappedSystemVa;
    uint32_t StartVa;
    ULONG ByteCount;
    ULONG ByteOffset;
};

static const uint32_t MDL_MAPPED_TO_SYSTEM_VA     = 0x0001;
static const uint32_t MDL_PAGES_LOCKED            = 0x0002;
static const uint32_t MDL_SOURCE_IS_NONPAGED_POOL = 0x0004;
static const uint32_t MDL_ALLOCATED_FIXED_SIZE    = 0x0008;
static const uint32_t MDL_PARTIAL                 = 0x0010;
static const uint32_t MDL_PARTIAL_HAS_BEEN_MAPPED = 0x0020;
static const uint32_t MDL_IO_PAGE_READ            = 0x0040;
static const uint32_t MDL_WRITE_OPERATION         = 0x0080;
static const uint32_t MDL_PARENT_MAPPED_SYSTEM_VA = 0x0100;
static const uint32_t MDL_FREE_EXTRA_PTES         = 0x0200;
static const uint32_t MDL_IO_SPACE                = 0x0800;
static const uint32_t MDL_NETWORK_HEADER          = 0x1000;
static const uint32_t MDL_MAPPING_CAN_FAIL        = 0x2000;
static const uint32_t MDL_ALLOCATED_MUST_SUCCEED  = 0x4000;


static const uint32_t MDL_MAPPING_FLAGS = (MDL_MAPPED_TO_SYSTEM_VA     |
                           MDL_PAGES_LOCKED            |
                           MDL_SOURCE_IS_NONPAGED_POOL |
                           MDL_PARTIAL_HAS_BEEN_MAPPED |
                           MDL_PARENT_MAPPED_SYSTEM_VA |
                    //       MDL_SYSTEM_VA               |
                           MDL_IO_SPACE );

static const uint32_t METHOD_BUFFERED   = 0;
static const uint32_t METHOD_IN_DIRECT  = 1;
static const uint32_t METHOD_OUT_DIRECT = 2;
static const uint32_t METHOD_NEITHER    = 3;

enum BUS_DATA_TYPE {
  ConfigurationSpaceUndefined = -1,
  Cmos,
  EisaConfiguration,
  Pos,
  CbusConfiguration,
  PCIConfiguration,
  VMEConfiguration,
  NuBusConfiguration,
  PCMCIAConfiguration,
  MPIConfiguration,
  MPSAConfiguration,
  PNPISAConfiguration,
  MaximumBusDataType
};


//
// HAL Bus Handler
//
struct BUS_HANDLER32
{
    uint32_t Version;
    s2e::windows::INTERFACE_TYPE InterfaceType;
    BUS_DATA_TYPE ConfigurationType;
    uint32_t BusNumber;
    uint32_t DeviceObject; //PDEVICE_OBJECT
    uint32_t ParentHandler; //struct _BUS_HANDLER *
    uint32_t BusData;  //PVOID
    uint32_t DeviceControlExtensionSize;
    //PSUPPORTED_RANGES BusAddresses;
    uint32_t Reserved[4];
#if 0
    pGetSetBusData GetBusData;
    pGetSetBusData SetBusData;
    pAdjustResourceList AdjustResourceList;
    pAssignSlotResources AssignSlotResources;
    pGetInterruptVector GetInterruptVector;
    pTranslateBusAddress TranslateBusAddress;
#endif
    void print(llvm::raw_ostream &os) const {
        os << "Version:           " << hexval(Version) << '\n';
        os << "InterfaceType:     " << hexval(InterfaceType) << '\n';
        os << "ConfigurationType: " << hexval(ConfigurationType) << '\n';
        os << "BusNumber:         " << hexval(BusNumber) << '\n';
        os << "DeviceObject:      " << hexval(DeviceObject) << '\n';
        os << "ParentHandler:     " << hexval(ParentHandler) << '\n';
        os << "BusData:           " << hexval(BusData) << '\n';
    }
};

struct LUID {
  uint32_t LowPart;
  uint32_t HighPart;
};

typedef uint32_t PSID;

struct SE_EXPORTS {
  LUID SeCreateTokenPrivilege;
  LUID SeAssignPrimaryTokenPrivilege;
  LUID SeLockMemoryPrivilege;
  LUID SeIncreaseQuotaPrivilege;
  LUID SeUnsolicitedInputPrivilege;
  LUID SeTcbPrivilege;
  LUID SeSecurityPrivilege;
  LUID SeTakeOwnershipPrivilege;
  LUID SeLoadDriverPrivilege;
  LUID SeCreatePagefilePrivilege;
  LUID SeIncreaseBasePriorityPrivilege;
  LUID SeSystemProfilePrivilege;
  LUID SeSystemtimePrivilege;
  LUID SeProfileSingleProcessPrivilege;
  LUID SeCreatePermanentPrivilege;
  LUID SeBackupPrivilege;
  LUID SeRestorePrivilege;
  LUID SeShutdownPrivilege;
  LUID SeDebugPrivilege;
  LUID SeAuditPrivilege;
  LUID SeSystemEnvironmentPrivilege;
  LUID SeChangeNotifyPrivilege;
  LUID SeRemoteShutdownPrivilege;
  PSID SeNullSid;
  PSID SeWorldSid;
  PSID SeLocalSid;
  PSID SeCreatorOwnerSid;
  PSID SeCreatorGroupSid;
  PSID SeNtAuthoritySid;
  PSID SeDialupSid;
  PSID SeNetworkSid;
  PSID SeBatchSid;
  PSID SeInteractiveSid;
  PSID SeLocalSystemSid;
  PSID SeAliasAdminsSid;
  PSID SeAliasUsersSid;
  PSID SeAliasGuestsSid;
  PSID SeAliasPowerUsersSid;
  PSID SeAliasAccountOpsSid;
  PSID SeAliasSystemOpsSid;
  PSID SeAliasPrintOpsSid;
  PSID SeAliasBackupOpsSid;
  PSID SeAuthenticatedUsersSid;
  PSID SeRestrictedSid;
  PSID SeAnonymousLogonSid;
  LUID SeUndockPrivilege;
  LUID SeSyncAgentPrivilege;
  LUID SeEnableDelegationPrivilege;
  PSID SeLocalServiceSid;
  PSID SeNetworkServiceSid;
  LUID SeManageVolumePrivilege;
  LUID SeImpersonatePrivilege;
  LUID SeCreateGlobalPrivilege;
  LUID SeTrustedCredManAccessPrivilege;
  LUID SeRelabelPrivilege;
  LUID SeIncreaseWorkingSetPrivilege;
  LUID SeTimeZonePrivilege;
  LUID SeCreateSymbolicLinkPrivilege;
  PSID SeIUserSid;
  PSID SeUntrustedMandatorySid;
  PSID SeLowMandatorySid;
  PSID SeMediumMandatorySid;
  PSID SeHighMandatorySid;
  PSID SeSystemMandatorySid;
  PSID SeOwnerRightsSid;
  PSID SeAllAppPackagesSid;
};

typedef ULONG DEVICE_TYPE;

typedef uint32_t KSPIN_LOCK;


struct KDEVICE_QUEUE32 {
    CSHORT Type;
    CSHORT Size;
    LIST_ENTRY32 DeviceListHead;
    KSPIN_LOCK Lock;

#if defined(_AMD64_)

    union {
        BOOLEAN Busy;
        struct {
            LONG64 Reserved : 8;
            LONG64 Hint : 56;
        };
    };

#else

    BOOLEAN Busy;

#endif

};


struct WAIT_CONTEXT_BLOCK32 {
    KDEVICE_QUEUE_ENTRY32 WaitQueueEntry;
    uint32_t DeviceRoutine; //PDRIVER_CONTROL
    uint32_t DeviceContext; //PVOID
    ULONG NumberOfMapRegisters;
    uint32_t DeviceObject; //PVOID
    uint32_t CurrentIrp; //PVOID
    uint32_t BufferChainingDpc; //PKDPC
};

struct KDPC32 {
    UCHAR Type;
    UCHAR Importance;
    volatile USHORT Number;
    LIST_ENTRY32 DpcListEntry;
    uint32_t DeferredRoutine; //PKDEFERRED_ROUTINE
    uint32_t DeferredContext; //PVOID
    uint32_t SystemArgument1; //PVOID
    uint32_t SystemArgument2; //PVOID
    uint32_t DpcData; //PVOID
};



#define TIMER_EXPIRED_INDEX_BITS        6
#define TIMER_PROCESSOR_INDEX_BITS      5

struct DISPATCHER_HEADER32 {
    union {
        struct {
            UCHAR Type;                 // All (accessible via KOBJECT_TYPE)

            union {
                union {                 // Timer
                    UCHAR TimerControlFlags;
                    struct {
                        UCHAR Absolute              : 1;
                        UCHAR Coalescable           : 1;
                        UCHAR KeepShifting          : 1;    // Periodic timer
                        UCHAR EncodedTolerableDelay : 5;    // Periodic timer
                    } DUMMYSTRUCTNAME;
                } DUMMYUNIONNAME;

                UCHAR Abandoned;        // Queue
                BOOLEAN Signalling;     // Gate/Events
            } DUMMYUNIONNAME;

            union {
                union {
                    UCHAR ThreadControlFlags;  // Thread
                    struct {
                        UCHAR CpuThrottled      : 1;
                        UCHAR CycleProfiling    : 1;
                        UCHAR CounterProfiling  : 1;
                        UCHAR Reserved          : 5;
                    } DUMMYSTRUCTNAME;
                } DUMMYUNIONNAME;
                UCHAR Hand;             // Timer
                UCHAR Size;             // All other objects
            } DUMMYUNIONNAME2;

            union {
                union {                 // Timer
                    UCHAR TimerMiscFlags;
                    struct {

#if !defined(_X86_)

                        UCHAR Index             : TIMER_EXPIRED_INDEX_BITS;

#else

                        UCHAR Index             : 1;
                        UCHAR Processor         : TIMER_PROCESSOR_INDEX_BITS;

#endif

                        UCHAR Inserted          : 1;
                        volatile UCHAR Expired  : 1;
                    } DUMMYSTRUCTNAME;
                } DUMMYUNIONNAME1;
                union {                 // Thread
                    BOOLEAN DebugActive;
                    struct {
                        BOOLEAN ActiveDR7       : 1;
                        BOOLEAN Instrumented    : 1;
                        BOOLEAN Reserved2       : 4;
                        BOOLEAN UmsScheduled    : 1;
                        BOOLEAN UmsPrimary      : 1;
                    } DUMMYSTRUCTNAME;
                } DUMMYUNIONNAME2;
                BOOLEAN DpcActive;      // Mutant
            } DUMMYUNIONNAME3;
        } DUMMYSTRUCTNAME;

        volatile LONG Lock;             // Interlocked
    } DUMMYUNIONNAME;

    LONG SignalState;                   // Object lock
    LIST_ENTRY32 WaitListHead;            // Object lock
};

struct KEVENT32 {
    DISPATCHER_HEADER32 Header;
};

struct DEVICE_OBJECT32 {
  CSHORT                      Type;
  USHORT                      Size;
  LONG                        ReferenceCount;
  uint32_t  DriverObject; //struct _DRIVER_OBJECT *
  uint32_t  NextDevice;   //struct _DEVICE_OBJECT *
  uint32_t  AttachedDevice; //struct _DEVICE_OBJECT *
  uint32_t  CurrentIrp; //struct _IRP *
  uint32_t                   Timer; //PIO_TIMER
  ULONG                       Flags;
  ULONG                       Characteristics;
  uint32_t             Vpb; //__volatile PVPB
  uint32_t                       DeviceExtension; //PVOID
  DEVICE_TYPE                 DeviceType;
  CCHAR                       StackSize;
  union {
    LIST_ENTRY32         ListEntry;
    WAIT_CONTEXT_BLOCK32 Wcb;
  } Queue;
  ULONG                       AlignmentRequirement;
  KDEVICE_QUEUE32               DeviceQueue;
  KDPC32                        Dpc;
  ULONG                       ActiveThreadCount;
  uint32_t        SecurityDescriptor; //PSECURITY_DESCRIPTOR
  KEVENT32                      DeviceLock;
  USHORT                      SectorSize;
  USHORT                      Spare1;
  uint32_t  DeviceObjectExtension; //struct _DEVOBJ_EXTENSION  *
  uint32_t                       Reserved; //PVOID
};

struct FILE_OBJECT32 {
    CSHORT Type;
    CSHORT Size;
    uint32_t DeviceObject; //PDEVICE_OBJECT
    uint32_t Vpb; //PVPB
    uint32_t FsContext; //PVOID
    uint32_t FsContext2; //PVOID
    uint32_t SectionObjectPointer; //PSECTION_OBJECT_POINTERS
    uint32_t PrivateCacheMap; //PVOID
    NTSTATUS FinalStatus;
    uint32_t RelatedFileObject; //struct _FILE_OBJECT *
    BOOLEAN LockOperation;
    BOOLEAN DeletePending;
    BOOLEAN ReadAccess;
    BOOLEAN WriteAccess;
    BOOLEAN DeleteAccess;
    BOOLEAN SharedRead;
    BOOLEAN SharedWrite;
    BOOLEAN SharedDelete;
    ULONG Flags;
    UNICODE_STRING32 FileName;
    uint64_t CurrentByteOffset; //LARGE_INTEGER
    __volatile ULONG Waiters;
    __volatile ULONG Busy;
    uint32_t LastLock; //PVOID
    KEVENT32 Lock;
    KEVENT32 Event;
    uint32_t CompletionContext; //__volatile PIO_COMPLETION_CONTEXT
    KSPIN_LOCK IrpListLock;
    LIST_ENTRY32 IrpList;
    uint32_t FileObjectExtension; //__volatile PVOID
};

struct KTIMER32 {
  DISPATCHER_HEADER32  Header;
  uint64_t  DueTime; //ULARGE_INTEGER
  LIST_ENTRY32  TimerListEntry;
  uint32_t Dpc; //struct _KDPC  *
  LONG  Period;
};

typedef uint64_t LARGE_INTEGER;

struct ETHREAD32
{
    uint8_t Tcb[0x1c0];
    LARGE_INTEGER CreateTime;
    union
    {
      LARGE_INTEGER ExitTime;
      LIST_ENTRY32 KeyedWaitChain;
    };
    union
    {
      LONG ExitStatus;
      uint32_t OfsChain;
    };

    LIST_ENTRY32 PostBlockList;

    union
    {
      uint32_t TerminationPort; //PTERMINATION_PORT
      uint32_t ReaperLink; //PETHREAD
      uint32_t KeyedWaitValue; //PETHREAD
    };

    ULONG ActiveTimerListLock;
    LIST_ENTRY32 ActiveTimerListHead;
    uint32_t Cid[2]; //CLIENT_ID


    union
    {
        uint8_t KeyedWaitSemaphore[0x14]; //KSEMAPHORE
        uint8_t AlpcWaitSemaphore[0x14]; //KSEMAPHORE
    };

    uint32_t LpcReplyMessage;

    uint32_t ImpersonationInfo;
    LIST_ENTRY32 IrpList;
    ULONG TopLevelIrp;
    uint32_t DeviceToVerify; //PDEVICE_OBJECT
    uint32_t ThreadsProcess; //PEPROCESS

    uint32_t StartAddress;
    uint32_t Win32StartAddress;
    LIST_ENTRY32 ThreadListEntry;
    //...
}__attribute__((packed));



/*
+0x000 Tcb              : _KTHREAD

   +0x1c0 CreateTime       : _LARGE_INTEGER
   +0x1c0 NestedFaultCount : Pos 0, 2 Bits
   +0x1c0 ApcNeeded        : Pos 2, 1 Bit

   +0x1c8 ExitTime         : _LARGE_INTEGER
   +0x1c8 LpcReplyChain    : _LIST_ENTRY
   +0x1c8 KeyedWaitChain   : _LIST_ENTRY

   +0x1d0 ExitStatus       : Int4B
   +0x1d0 OfsChain         : Ptr32 Void

   +0x1d4 PostBlockList    : _LIST_ENTRY

   +0x1dc TerminationPort  : Ptr32 _TERMINATION_PORT
   +0x1dc ReaperLink       : Ptr32 _ETHREAD
   +0x1dc KeyedWaitValue   : Ptr32 Void

   +0x1e0 ActiveTimerListLock : Uint4B
   +0x1e4 ActiveTimerListHead : _LIST_ENTRY
   +0x1ec Cid              : _CLIENT_ID

   +0x1f4 LpcReplySemaphore : _KSEMAPHORE
   +0x1f4 KeyedWaitSemaphore : _KSEMAPHORE

   +0x208 LpcReplyMessage  : Ptr32 Void
   +0x208 LpcWaitingOnPort : Ptr32 Void

   +0x20c ImpersonationInfo : Ptr32 _PS_IMPERSONATION_INFORMATION
   +0x210 IrpList          : _LIST_ENTRY
   +0x218 TopLevelIrp      : Uint4B
   +0x21c DeviceToVerify   : Ptr32 _DEVICE_OBJECT
   +0x220 ThreadsProcess   : Ptr32 _EPROCESS
   +0x224 StartAddress     : Ptr32 Void
   +0x228 Win32StartAddress : Ptr32 Void
   +0x228 LpcReceivedMessageId : Uint4B
   +0x22c ThreadListEntry  : _LIST_ENTRY

*/

static const uint32_t ETHREAD32_THREADLISTENTRY_OFFSET_XP = 0x22c;
static const uint32_t EPROCESS32_THREADLISTHEAD_OFFSET_XP = 0x190;

static const uint32_t ETHREAD32_SIZE = 0x234;

}
}

#endif
