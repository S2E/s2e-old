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

/**
 *  Defines structures and utility classes to parse
 *  windows PE images.
 */

#ifndef _WINDOWS_IMAGE_H_

#define _WINDOWS_IMAGE_H_

#include <inttypes.h>
#include <map>
#include <string>

namespace s2e {
namespace windows {

static const uint16_t IMAGE_DOS_SIGNATURE = 0x5A4D;      // MZ
static const uint32_t IMAGE_NT_SIGNATURE = 0x00004550;  // PE00

typedef struct IMAGE_DOS_HEADER {      // DOS .EXE header
    uint16_t   e_magic;                     // Magic number
    uint16_t   e_cblp;                      // Bytes on last page of file
    uint16_t   e_cp;                        // Pages in file
    uint16_t   e_crlc;                      // Relocations
    uint16_t   e_cparhdr;                   // Size of header in paragraphs
    uint16_t   e_minalloc;                  // Minimum extra paragraphs needed
    uint16_t   e_maxalloc;                  // Maximum extra paragraphs needed
    uint16_t   e_ss;                        // Initial (relative) SS value
    uint16_t   e_sp;                        // Initial SP value
    uint16_t   e_csum;                      // Checksum
    uint16_t   e_ip;                        // Initial IP value
    uint16_t   e_cs;                        // Initial (relative) CS value
    uint16_t   e_lfarlc;                    // File address of relocation table
    uint16_t   e_ovno;                      // Overlay number
    uint16_t   e_res[4];                    // Reserved uint16_ts
    uint16_t   e_oemid;                     // OEM identifier (for e_oeminfo)
    uint16_t   e_oeminfo;                   // OEM information; e_oemid specific
    uint16_t   e_res2[10];                  // Reserved uint16_ts
    int32_t    e_lfanew;                    // File address of new exe header
  }__attribute__ ((packed)) IMAGE_DOS_HEADER;

typedef struct IMAGE_FILE_HEADER {	//20 bytes
    uint16_t    Machine;
    uint16_t    NumberOfSections;
    uint32_t   TimeDateStamp;
    uint32_t   PointerToSymbolTable;
    uint32_t   NumberOfSymbols;
    uint16_t    SizeOfOptionalHeader;
    uint16_t    Characteristics;
} __attribute__ ((packed)) IMAGE_FILE_HEADER;

#define IMAGE_SIZEOF_FILE_HEADER             20


#if 0
#define IMAGE_FILE_RELOCS_STRIPPED           0x0001  // Relocation info stripped from file.
#define IMAGE_FILE_EXECUTABLE_IMAGE          0x0002  // File is executable  (i.e. no unresolved externel references).
#define IMAGE_FILE_LINE_NUMS_STRIPPED        0x0004  // Line nunbers stripped from file.
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED       0x0008  // Local symbols stripped from file.
#define IMAGE_FILE_AGGRESIVE_WS_TRIM         0x0010  // Agressively trim working set
#define IMAGE_FILE_LARGE_ADDRESS_AWARE       0x0020  // App can handle >2gb addresses
#define IMAGE_FILE_BYTES_REVERSED_LO         0x0080  // Bytes of machine uint16_t are reversed.
#define IMAGE_FILE_32BIT_MACHINE             0x0100  // 32 bit uint16_t machine.
#define IMAGE_FILE_DEBUG_STRIPPED            0x0200  // Debugging info stripped from file in .DBG file
#define IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP   0x0400  // If Image is on removable media, copy and run from the swap file.
#define IMAGE_FILE_NET_RUN_FROM_SWAP         0x0800  // If Image is on Net, copy and run from the swap file.
#define IMAGE_FILE_SYSTEM                    0x1000  // System File.
#define IMAGE_FILE_DLL                       0x2000  // File is a DLL.
#define IMAGE_FILE_UP_SYSTEM_ONLY            0x4000  // File should only be run on a UP machine
#define IMAGE_FILE_BYTES_REVERSED_HI         0x8000  // Bytes of machine uint16_t are reversed.

#define IMAGE_FILE_MACHINE_UNKNOWN           0
#define IMAGE_FILE_MACHINE_I386              0x014c  // Intel 386.
#define IMAGE_FILE_MACHINE_R3000             0x0162  // MIPS little-endian, 0x160 big-endian
#define IMAGE_FILE_MACHINE_R4000             0x0166  // MIPS little-endian
#define IMAGE_FILE_MACHINE_R10000            0x0168  // MIPS little-endian
#define IMAGE_FILE_MACHINE_WCEMIPSV2         0x0169  // MIPS little-endian WCE v2
#define IMAGE_FILE_MACHINE_ALPHA             0x0184  // Alpha_AXP
#define IMAGE_FILE_MACHINE_POWERPC           0x01F0  // IBM PowerPC Little-Endian
#define IMAGE_FILE_MACHINE_SH3               0x01a2  // SH3 little-endian
#define IMAGE_FILE_MACHINE_SH3E              0x01a4  // SH3E little-endian
#define IMAGE_FILE_MACHINE_SH4               0x01a6  // SH4 little-endian
#define IMAGE_FILE_MACHINE_ARM               0x01c0  // ARM Little-Endian
#define IMAGE_FILE_MACHINE_THUMB             0x01c2
#define IMAGE_FILE_MACHINE_IA64              0x0200  // Intel 64
#define IMAGE_FILE_MACHINE_MIPS16            0x0266  // MIPS
#define IMAGE_FILE_MACHINE_MIPSFPU           0x0366  // MIPS
#define IMAGE_FILE_MACHINE_MIPSFPU16         0x0466  // MIPS
#define IMAGE_FILE_MACHINE_ALPHA64           0x0284  // ALPHA64
#define IMAGE_FILE_MACHINE_AXP64             IMAGE_FILE_MACHINE_ALPHA64
#endif
//
// Directory format.
//

typedef struct IMAGE_DATA_DIRECTORY {
    uint32_t   VirtualAddress;
    uint32_t   Size;
} __attribute__ ((packed)) IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES    16

//
// Optional header format.
//

typedef struct IMAGE_OPTIONAL_HEADER {			//96 + ... bytes
    //
    // Standard fields.
    //

    uint16_t    Magic;
    uint8_t    MajorLinkerVersion;
    uint8_t    MinorLinkerVersion;
    uint32_t   SizeOfCode;
    uint32_t   SizeOfInitializedData;
    uint32_t   SizeOfUninitializedData;
    uint32_t   AddressOfEntryPoint;			//+16 bytes
    uint32_t   BaseOfCode;
    uint32_t   BaseOfData;

    //
    // NT additional fields.
    //

    uint32_t   ImageBase;
    uint32_t   SectionAlignment;
    uint32_t   FileAlignment;
    uint16_t    MajorOperatingSystemVersion;
    uint16_t    MinorOperatingSystemVersion;
    uint16_t    MajorImageVersion;
    uint16_t    MinorImageVersion;
    uint16_t    MajorSubsystemVersion;
    uint16_t    MinorSubsystemVersion;
    uint32_t   Win32VersionValue;
    uint32_t   SizeOfImage;
    uint32_t   SizeOfHeaders;
    uint32_t   CheckSum;
    uint16_t    Subsystem;
    uint16_t    DllCharacteristics;
    uint32_t   SizeOfStackReserve;
    uint32_t   SizeOfStackCommit;
    uint32_t   SizeOfHeapReserve;
    uint32_t   SizeOfHeapCommit;
    uint32_t   LoaderFlags;
    uint32_t   NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} __attribute__ ((packed)) IMAGE_OPTIONAL_HEADER;


typedef struct IMAGE_NT_HEADERS {				//
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER OptionalHeader;
} __attribute__ ((packed)) IMAGE_NT_HEADERS;

typedef struct IMAGE_ROM_OPTIONAL_HEADER {
    uint16_t   Magic;
    uint8_t   MajorLinkerVersion;
    uint8_t   MinorLinkerVersion;
    uint32_t  SizeOfCode;
    uint32_t  SizeOfInitializedData;
    uint32_t  SizeOfUninitializedData;
    uint32_t  AddressOfEntryPoint;
    uint32_t  BaseOfCode;
    uint32_t  BaseOfData;
    uint32_t  BaseOfBss;
    uint32_t  GprMask;
    uint32_t  CprMask[4];
    uint32_t  GpValue;
} __attribute__ ((packed)) IMAGE_ROM_OPTIONAL_HEADER;


#define IMAGE_SIZEOF_ROM_OPTIONAL_HEADER      56
#define IMAGE_SIZEOF_STD_OPTIONAL_HEADER      28
#define IMAGE_SIZEOF_NT_OPTIONAL32_HEADER    224
#define IMAGE_SIZEOF_NT_OPTIONAL64_HEADER    240

#define IMAGE_NT_OPTIONAL_HDR32_MAGIC      0x10b
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC      0x20b
#define IMAGE_ROM_OPTIONAL_HDR_MAGIC       0x107


#define IMAGE_SIZEOF_SHORT_NAME              8

typedef struct IMAGE_SECTION_HEADER {
    uint8_t    Name[IMAGE_SIZEOF_SHORT_NAME];
    union {
            uint32_t   PhysicalAddress;
            uint32_t   VirtualSize;
    } Misc;
    uint32_t   VirtualAddress;
    uint32_t   SizeOfRawData;
    uint32_t   PointerToRawData;
    uint32_t   PointerToRelocations;
    uint32_t   PointerToLinenumbers;
    uint16_t    NumberOfRelocations;
    uint16_t    NumberOfLinenumbers;
    uint32_t   Characteristics;
}  __attribute__ ((packed)) IMAGE_SECTION_HEADER;

static const uint32_t IMAGE_SCN_MEM_WRITE = 0x80000000;
static const uint32_t IMAGE_SCN_MEM_READ = 0x40000000;
static const uint32_t IMAGE_SCN_MEM_EXECUTE = 0x20000000;

#define IMAGE_SIZEOF_SECTION_HEADER          40

// Directory Entries

#define IMAGE_DIRECTORY_ENTRY_EXPORT          0   // Export Directory
#define IMAGE_DIRECTORY_ENTRY_IMPORT          1   // Import Directory
#define IMAGE_DIRECTORY_ENTRY_RESOURCE        2   // Resource Directory
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION       3   // Exception Directory
#define IMAGE_DIRECTORY_ENTRY_SECURITY        4   // Security Directory
#define IMAGE_DIRECTORY_ENTRY_BASERELOC       5   // Base Relocation Table
#define IMAGE_DIRECTORY_ENTRY_DEBUG           6   // Debug Directory
//      IMAGE_DIRECTORY_ENTRY_COPYRIGHT       7   // (X86 usage)
#define IMAGE_DIRECTORY_ENTRY_ARCHITECTURE    7   // Architecture Specific Data
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR       8   // RVA of GP
#define IMAGE_DIRECTORY_ENTRY_TLS             9   // TLS Directory
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG    10   // Load Configuration Directory
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT   11   // Bound Import Directory in headers
#define IMAGE_DIRECTORY_ENTRY_IAT            12   // Import Address Table
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT   13   // Delay Load Import Descriptors
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14   // COM Runtime descriptor


//
// DLL support.
//

//
// Export Format
//

typedef struct IMAGE_EXPORT_DIRECTORY {
    uint32_t   Characteristics;
    uint32_t   TimeDateStamp;
    uint16_t    MajorVersion;
    uint16_t    MinorVersion;
    uint32_t   Name;
    uint32_t   Base;
    uint32_t   NumberOfFunctions;
    uint32_t   NumberOfNames;
    uint32_t   AddressOfFunctions;     // RVA from base of image
    uint32_t   AddressOfNames;         // RVA from base of image
    uint32_t   AddressOfNameOrdinals;  // RVA from base of image
} __attribute__ ((packed)) IMAGE_EXPORT_DIRECTORY, *PIMAGE_EXPORT_DIRECTORY;

//
// Import Format
//

typedef struct IMAGE_IMPORT_BY_NAME {
    uint16_t    Hint;
    uint8_t    Name[1];
} __attribute__ ((packed)) IMAGE_IMPORT_BY_NAME;

#ifndef IMAGE_ORDINAL_FLAG
#define IMAGE_ORDINAL_FLAG  0x80000000
#endif

typedef struct IMAGE_THUNK_DATA {
    union {
        uint32_t ForwarderString; //PBYTE
        uint32_t Function; //Puint32_t
        uint32_t Ordinal;
        uint32_t AddressOfData; //IMAGE_IMPORT_BY_NAME  *
    } u1;
} __attribute__ ((packed)) IMAGE_THUNK_DATA, *PIMAGE_THUNK_DATA, IMAGE_THUNK_DATA32, *PIMAGE_THUNK_DATA32;

typedef IMAGE_THUNK_DATA * PIMAGE_THUNK_DATA;

typedef struct IMAGE_IMPORT_DESCRIPTOR {
    union {
        uint32_t   Characteristics;            // 0 for terminating null import descriptor
        uint32_t   OriginalFirstThunk;         // RVA to original unbound IAT (PIMAGE_THUNK_DATA)
    };
    uint32_t   TimeDateStamp;                  // 0 if not bound,
                                            // -1 if bound, and real date\time stamp
                                            //     in IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT (new BIND)
                                            // O.W. date/time stamp of DLL bound to (Old BIND)

    uint32_t   ForwarderChain;                 // -1 if no forwarders
    uint32_t   Name;
    uint32_t   FirstThunk;                     // RVA to IAT (if bound this IAT has actual addresses)
} __attribute__ ((packed))IMAGE_IMPORT_DESCRIPTOR, *PIMAGE_IMPORT_DESCRIPTOR;


//
// Based relocation format.
//

typedef struct IMAGE_BASE_RELOCATION {
    uint32_t   VirtualAddress;
    uint32_t   SizeOfBlock;
//  uint16_t    TypeOffset[1];
} __attribute__ ((packed)) IMAGE_BASE_RELOCATION;

typedef IMAGE_BASE_RELOCATION * PIMAGE_BASE_RELOCATION;

#define IMAGE_SIZEOF_BASE_RELOCATION         8

//
// Based relocation types.
//

#define IMAGE_REL_BASED_ABSOLUTE              0
#define IMAGE_REL_BASED_HIGH                  1
#define IMAGE_REL_BASED_LOW                   2
#define IMAGE_REL_BASED_HIGHLOW               3
#define IMAGE_REL_BASED_HIGHADJ               4
#define IMAGE_REL_BASED_MIPS_JMPADDR          5
#define IMAGE_REL_BASED_SECTION               6
#define IMAGE_REL_BASED_REL32                 7

#define IMAGE_REL_BASED_MIPS_JMPADDR16        9
#define IMAGE_REL_BASED_IA64_IMM64            9
#define IMAGE_REL_BASED_DIR64                 10
#define IMAGE_REL_BASED_HIGH3ADJ              11

typedef struct IMAGE_RELOC_TYPE
{
    unsigned offset:12;
    unsigned type:4;


}__attribute__((packed))IMAGE_RELOC_TYPE;

/****************************************************************/
/****************************************************************/
/****************************************************************/

/*typedef struct _UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR  Buffer;
}UNICODE_STRING, *PUNICODE_STRING;*/

typedef struct _UNICODE_STRING32 {
  uint16_t Length;
  uint16_t MaximumLength;
  uint32_t  Buffer;
}UNICODE_STRING32, *PUNICODE_STRING32;

typedef struct _BINARY_DATA32 {
  uint16_t Length;
  uint32_t Buffer;
} __attribute__((packed)) BINARY_DATA32, *PBINARY_DATA32;

typedef struct _LIST_ENTRY32 {
    uint32_t Flink;
    uint32_t Blink;
}LIST_ENTRY32, *PLIST_ENTRY32;

#define CONTAINING_RECORD32(address, type, field) ((uint32_t)( \
                                                  (uint32_t)(address) - \
                                                  (uint32_t)(uint64_t)(&((type *)0)->field)))

typedef int32_t NTSTATUS; //MUST BE SIGNED

#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#define NT_INFORMATION(Status) ((ULONG)(Status) >> 30 == 1)
#define NT_WARNING(Status) ((ULONG)(Status) >> 30 == 2)
#define NT_ERROR(Status) ((ULONG)(Status) >> 30 == 3)

typedef struct _MODULE_ENTRY32
{
    LIST_ENTRY32 le_mod;
    uint32_t  unknown[4];
    uint32_t  base;
    uint32_t  driver_start;
    uint32_t  unk1;
    UNICODE_STRING32 driver_Path;
    UNICODE_STRING32 driver_Name;
}   __attribute__((packed)) MODULE_ENTRY32, *PMODULE_ENTRY32;

typedef struct _DRIVER_OBJECT32
{
  uint16_t Type;
  uint16_t Size;

  uint32_t DeviceObject; //PVOID
  uint32_t Flags;

  uint32_t DriverStart; //PVOID
  uint32_t DriverSize; //PVOID
  uint32_t DriverSection; //PVOID
  UNICODE_STRING32 DriverName;

  PUNICODE_STRING32 HardwareDatabase;
  uint32_t FastIoDispatch;
  uint32_t DriverInit;
  uint32_t DriverStartIo;
  uint32_t DriverUnload;
  uint32_t MajorFunction[28];
} __attribute__((packed)) DRIVER_OBJECT32, *PDRIVER_OBJECT32;

extern const char * s_irpMjArray [];

//KPCR is at fs:1c
//This is only valid for XP (no ASLR)
//#define KPCR_ADDRESS  0xFFDFF000

//Offset of the pointer to KPCR relative to the fs register
#define KPCR_FS_OFFSET 0x1c

//Offset of the DBGKD_GET_VERSION32 data structure in the KPCR
#define KPCR_KDVERSION32_OFFSET 0x34

//Offset of the KPRCB in the KPCR
#define KPCR_KPRCB_OFFSET 0x120
#define KPCR_KPRCB_PTR_OFFSET 0x20


//Offset of the current thread in the FS register
#define FS_CURRENT_THREAD_OFFSET 0x124

//Offset of the pointer to the EPROCESS in the ETHREAD structure
#define ETHREAD_PROCESS_OFFSET_VISTA 0x48
#define ETHREAD_PROCESS_OFFSET_XP 0x44

#define EPROCESS_ACTIVE_PROCESS_LINK_XP 0x88


//#define KD_VERSION_BLOCK (KPCR_ADDRESS + 0x34)
#define PS_LOADED_MODULE_LIST_OFFSET 0x70 //Inside the kd version block

#define BUILD_WINXP     2600
#define BUILD_LONGHORN  5048


//#define KPRCB_OFFSET 0xFFDFF120
#define IRQL_OFFSET 0xFFDFF124
#define PEB_OFFSET 0x7FFDF000
typedef uint32_t KAFFINITY;

typedef struct _DBGKD_GET_VERSION32 {
    uint16_t    MajorVersion;   // 0xF == Free, 0xC == Checked
    uint16_t    MinorVersion;
    uint16_t    ProtocolVersion;
    uint16_t    Flags;          // DBGKD_VERS_FLAG_XXX
    uint32_t    KernBase;
    uint32_t    PsLoadedModuleList;
    uint16_t    MachineType;
    uint16_t    ThCallbackStack;
    uint16_t    NextCallback;
    uint16_t    FramePointer;
    uint32_t    KiCallUserMode;
    uint32_t    KeUserCallbackDispatcher;
    uint32_t    BreakpointWithStatus;
    uint32_t    Reserved4;
} __attribute__((packed)) DBGKD_GET_VERSION32, *PDBGKD_GET_VERSION32;

typedef struct _DBGKD_GET_VERSION64
{
     uint16_t MajorVersion;
     uint16_t MinorVersion;
     uint8_t ProtocolVersion;
     uint8_t KdSecondaryVersion;
     uint16_t Flags;
     uint16_t MachineType;
     uint8_t MaxPacketType;
     uint8_t MaxStateChange;
     uint8_t MaxManipulate;
     uint8_t Simulation;
     uint16_t Unused[1];
     uint64_t KernBase;
     uint64_t PsLoadedModuleList;
     uint64_t DebuggerDataList;
} __attribute__((packed)) DBGKD_GET_VERSION64, *PDBGKD_GET_VERSION64;

typedef struct _LDR_DATA_TABLE_ENTRY32
{
     LIST_ENTRY32 InLoadOrderLinks;
     LIST_ENTRY32 InMemoryOrderLinks;
     LIST_ENTRY32 InInitializationOrderLinks;
     uint32_t DllBase;
     uint32_t EntryPoint;
     uint32_t SizeOfImage;
     UNICODE_STRING32 FullDllName;
     UNICODE_STRING32 BaseDllName;
     uint32_t Flags;
     uint16_t LoadCount;
     uint16_t TlsIndex;
     union
     {
          LIST_ENTRY32 HashLinks;
          struct
          {
               uint32_t SectionPointer;
               uint32_t CheckSum;
          };
     };
     union
     {
          uint32_t TimeDateStamp;
          uint32_t LoadedImports;
     };
     uint32_t EntryPointActivationContext;
     uint32_t PatchInformation;
}  __attribute__((packed)) LDR_DATA_TABLE_ENTRY32, *PLDR_DATA_TABLE_ENTRY32;


typedef struct _PEB_LDR_DATA32
{
  uint32_t Length;
  uint32_t Initialized;
  uint32_t SsHandle;
  LIST_ENTRY32 InLoadOrderModuleList;
  LIST_ENTRY32 InMemoryOrderModuleList;
  uint32_t EntryInProgress;
}  __attribute__((packed))PEB_LDR_DATA32;

typedef struct _PEB32 {
  uint8_t Unk1[0x8];
  uint32_t ImageBaseAddress;
  uint32_t Ldr; /* PEB_LDR_DATA */
} __attribute__((packed))PEB32;


typedef struct _KPROCESS32_XP {
  uint8_t Unk1[0x18];
  uint32_t DirectoryTableBase;
  uint8_t Unk2[0x50];
} __attribute__((packed))KPROCESS32_XP;

typedef struct _EPROCESS32_XP {
  KPROCESS32_XP Pcb;
  uint32_t ProcessLock;
  uint64_t CreateTime;
  uint64_t ExitTime;
  uint32_t RundownProtect;
  uint32_t UniqueProcessId;
  LIST_ENTRY32 ActiveProcessLinks;
  uint8_t Unk2[0xE4];
  uint8_t ImageFileName[16]; //offset 0x174
  uint32_t Unk3[11];
  uint32_t Peb;
} __attribute__((packed)) EPROCESS32_XP;

typedef struct _KPROCESS32_VISTA {
  uint8_t Unk1[0x10];
  LIST_ENTRY32 ProfileListHead;
  uint32_t DirectoryTableBase;
  uint8_t Unk2[0x64];
} __attribute__((packed))KPROCESS32_VISTA;

typedef struct _EPROCESS32_VISTA {
  KPROCESS32_VISTA Pcb;
  uint64_t ProcessLock;
  uint64_t CreateTime;
  uint64_t ExitTime;
  uint32_t RundownProtect;
  uint32_t UniqueProcessId;
  LIST_ENTRY32 ActiveProcessLinks;
  uint8_t Unk2[0xa4];
  uint8_t ImageFileName[16]; //offset 14c
  uint32_t Unk3[11];
  uint32_t Peb;
} __attribute__((packed)) EPROCESS32_VISTA;

typedef struct _KAPC_STATE32 {
  LIST_ENTRY32 ApcListHead[2];
  uint32_t Process;  /* Ptr to (E)KPROCESS */
  uint8_t KernelApcInProgress;
  uint8_t KernelApcPending;
  uint8_t UserApcPending;
} __attribute__((packed))KAPC_STATE32;

typedef struct _KTHREAD32
{
    uint8_t Unk1[0x18];
    uint32_t InitialStack;
    uint32_t StackLimit;
    uint8_t Unk2[0x14];
    KAPC_STATE32 ApcState;

    uint8_t Unk3[0x164];

    LIST_ENTRY32 ThreadListEntry;

} __attribute__((packed))KTHREAD32;

/*
+0x000 Header           : _DISPATCHER_HEADER
   +0x010 MutantListHead   : _LIST_ENTRY
   +0x018 InitialStack     : Ptr32 Void
   +0x01c StackLimit       : Ptr32 Void
   +0x020 Teb              : Ptr32 Void
   +0x024 TlsArray         : Ptr32 Void
   +0x028 KernelStack      : Ptr32 Void
   +0x02c DebugActive      : UChar
   +0x02d State            : UChar
   +0x02e Alerted          : [2] UChar
   +0x030 Iopl             : UChar
   +0x031 NpxState         : UChar
   +0x032 Saturation       : Char
   +0x033 Priority         : Char
   +0x034 ApcState         : _KAPC_STATE
   +0x04c ContextSwitches  : Uint4B
   +0x050 IdleSwapBlock    : UChar
   +0x051 Spare0           : [3] UChar
   +0x054 WaitStatus       : Int4B
   +0x058 WaitIrql         : UChar
   +0x059 WaitMode         : Char
   +0x05a WaitNext         : UChar
   +0x05b WaitReason       : UChar

   +0x05c WaitBlockList    : Ptr32 _KWAIT_BLOCK
   +0x060 WaitListEntry    : _LIST_ENTRY
   +0x060 SwapListEntry    : _SINGLE_LIST_ENTRY
   +0x068 WaitTime         : Uint4B
   +0x06c BasePriority     : Char
   +0x06d DecrementCount   : UChar
   +0x06e PriorityDecrement : Char
   +0x06f Quantum          : Char
   +0x070 WaitBlock        : [4] _KWAIT_BLOCK
   +0x0d0 LegoData         : Ptr32 Void
   +0x0d4 KernelApcDisable : Uint4B
   +0x0d8 UserAffinity     : Uint4B
   +0x0dc SystemAffinityActive : UChar
   +0x0dd PowerState       : UChar
   +0x0de NpxIrql          : UChar
   +0x0df InitialNode      : UChar
   +0x0e0 ServiceTable     : Ptr32 Void
   +0x0e4 Queue            : Ptr32 _KQUEUE
   +0x0e8 ApcQueueLock     : Uint4B
   +0x0f0 Timer            : _KTIMER
   +0x118 QueueListEntry   : _LIST_ENTRY
   +0x120 SoftAffinity     : Uint4B
   +0x124 Affinity         : Uint4B
   +0x128 Preempted        : UChar
   +0x129 ProcessReadyQueue : UChar
   +0x12a KernelStackResident : UChar
   +0x12b NextProcessor    : UChar
   +0x12c CallbackStack    : Ptr32 Void
   +0x130 Win32Thread      : Ptr32 Void
   +0x134 TrapFrame        : Ptr32 _KTRAP_FRAME
   +0x138 ApcStatePointer  : [2] Ptr32 _KAPC_STATE
   +0x140 PreviousMode     : Char
   +0x141 EnableStackSwap  : UChar
   +0x142 LargeStack       : UChar
   +0x143 ResourceIndex    : UChar
   +0x144 KernelTime       : Uint4B
   +0x148 UserTime         : Uint4B
   +0x14c SavedApcState    : _KAPC_STATE
   +0x164 Alertable        : UChar
   +0x165 ApcStateIndex    : UChar
   +0x166 ApcQueueable     : UChar
   +0x167 AutoAlignment    : UChar
   +0x168 StackBase        : Ptr32 Void
   +0x16c SuspendApc       : _KAPC
   +0x19c SuspendSemaphore : _KSEMAPHORE
   +0x1b0 ThreadListEntry  : _LIST_ENTRY
   +0x1b8 FreezeCount      : Char
   +0x1b9 SuspendCount     : Char
   +0x1ba IdealProcessor   : UChar
   +0x1bb DisableBoost     : UChar
*/

typedef struct _NT_TIB32
{
     uint32_t ExceptionList;  //PEXCEPTION_REGISTRATION_RECORD
     uint32_t StackBase;   //PVOID
     uint32_t StackLimit; //PVOID
     uint32_t SubSystemTib; //PVOID
     union
     {
          uint32_t FiberData; //PVOID
          uint32_t Version; //ULONG
     };
     uint32_t ArbitraryUserPointer;
     uint32_t Self; //PNT_TIB
}__attribute__((packed)) NT_TIB32;


struct DESCRIPTOR32
{
     uint16_t Pad;
     uint16_t Limit;
     uint32_t Base;
}__attribute__((packed));

struct KSPECIAL_REGISTERS32
{
     uint32_t Cr0;
     uint32_t Cr2;
     uint32_t Cr3;
     uint32_t Cr4;
     uint32_t KernelDr0;
     uint32_t KernelDr1;
     uint32_t KernelDr2;
     uint32_t KernelDr3;
     uint32_t KernelDr6;
     uint32_t KernelDr7;
     DESCRIPTOR32 Gdtr;
     DESCRIPTOR32 Idtr;
     uint16_t Tr;
     uint16_t Ldtr;
     uint32_t Reserved[6];
}__attribute__((packed));


typedef enum _INTERFACE_TYPE {
    InterfaceTypeUndefined = -1,
    Internal,
    Isa,
    Eisa,
    MicroChannel,
    TurboChannel,
    PCIBus,
    VMEBus,
    NuBus,
    PCMCIABus,
    CBus,
    MPIBus,
    MPSABus,
    ProcessorInternal,
    InternalPowerBus,
    PNPISABus,
    PNPBus,
    MaximumInterfaceType
}INTERFACE_TYPE, *PINTERFACE_TYPE;

struct FLOATING_SAVE_AREA
{
     uint32_t ControlWord;
     uint32_t StatusWord;
     uint32_t TagWord;
     uint32_t ErrorOffset;
     uint32_t ErrorSelector;
     uint32_t DataOffset;
     uint32_t DataSelector;
     uint8_t  RegisterArea[80];
     uint32_t Cr0NpxState;
}__attribute__((packed));


struct CONTEXT32
{
     uint32_t ContextFlags;
     uint32_t Dr0;
     uint32_t Dr1;
     uint32_t Dr2;
     uint32_t Dr3;
     uint32_t Dr6;
     uint32_t Dr7;
     FLOATING_SAVE_AREA FloatSave;
     uint32_t SegGs;
     uint32_t SegFs;
     uint32_t SegEs;
     uint32_t SegDs;
     uint32_t Edi;
     uint32_t Esi;
     uint32_t Ebx;
     uint32_t Edx;
     uint32_t Ecx;
     uint32_t Eax;
     uint32_t Ebp;
     uint32_t Eip;
     uint32_t SegCs;
     uint32_t EFlags;
     uint32_t Esp;
     uint32_t SegSs;
     uint8_t ExtendedRegisters[512];
}__attribute__((packed));

#define CONTEXT_i386    0x00010000
#define CONTEXT_i486    0x00010000

#define CONTEXT_CONTROL         (CONTEXT_i386 | 0x00000001L)
#define CONTEXT_INTEGER         (CONTEXT_i386 | 0x00000002L)
#define CONTEXT_SEGMENTS        (CONTEXT_i386 | 0x00000004L)
#define CONTEXT_FLOATING_POINT  (CONTEXT_i386 | 0x00000008L)
#define CONTEXT_DEBUG_REGISTERS (CONTEXT_i386 | 0x00000010L)
#define CONTEXT_EXTENDED_REGISTERS  (CONTEXT_i386 | 0x00000020L)

#define CONTEXT_FULL (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS)


#define EXCEPTION_MAXIMUM_PARAMETERS 15

#define EXCEPTION_NONCONTINUABLE   0x0001
#define EXCEPTION_UNWINDING        0x0002
#define EXCEPTION_EXIT_UNWIND      0x0004
#define EXCEPTION_STACK_INVALID    0x0008
#define EXCEPTION_NESTED_CALL      0x0010
#define EXCEPTION_TARGET_UNWIND    0x0020
#define EXCEPTION_COLLIDED_UNWIND  0x0040
#define EXCEPTION_UNWIND           0x0066

#define STATUS_BREAKPOINT 0x80000003

struct EXCEPTION_RECORD32 {
    uint32_t ExceptionCode;
    uint32_t ExceptionFlags;
    uint32_t ExceptionRecord; //struct _EXCEPTION_RECORD
    uint32_t ExceptionAddress; //PVOID
    uint32_t NumberParameters;
    uint32_t ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
}__attribute__((packed));

struct KPROCESSOR_STATE32
{
     CONTEXT32 ContextFrame;
     KSPECIAL_REGISTERS32 SpecialRegisters;
}__attribute__((packed));


static const uint32_t KPRCB32_DPC_STACK_OFFSET = 0x868;
struct KPRCB32 {
    uint16_t MinorVersion;
    uint16_t MajorVersion;
    uint32_t CurrentThread;
    uint32_t NextThread;
    uint32_t IdleThread;
    uint8_t Number;
    uint8_t WakeIdle;
    uint16_t BuildType;
    uint32_t SetMember;
    uint32_t  RestartBlock;

    KPROCESSOR_STATE32 ProcessorState;

} __attribute__((packed));

// Page frame number
typedef uint32_t PFN_NUMBER;

struct PHYSICAL_MEMORY_RUN {
    PFN_NUMBER BasePage;
    PFN_NUMBER PageCount;
}__attribute__((packed));

struct PHYSICAL_MEMORY_DESCRIPTOR {
    uint32_t NumberOfRuns;
    PFN_NUMBER NumberOfPages;
    PHYSICAL_MEMORY_RUN Run[1];
}__attribute__((packed));

static const uint32_t STATUS_SUCCESS = 0;
static const uint32_t STATUS_PENDING = 0x00000103;
static const uint32_t STATUS_BUFFER_TOO_SMALL = 0xC0000023;
static const uint32_t STATUS_UNKNOWN_REVISION = 0xC0000058;
static const uint32_t STATUS_INVALID_SECURITY_DESCR = 0xC0000079;
static const uint32_t STATUS_BAD_DESCRIPTOR_FORMAT = 0xC00000E7;

typedef uint32_t PACL32;
typedef uint32_t PSID32;
typedef uint16_t SECURITY_DESCRIPTOR_CONTROL;
typedef uint32_t PDEVICE_OBJECT32;
typedef uint8_t KPROCESSOR_MODE;
typedef uint8_t BOOLEAN;
typedef uint8_t UCHAR;
typedef int8_t CCHAR;
typedef uint16_t USHORT;
typedef uint16_t CSHORT;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef uint32_t UINT;
typedef uint32_t HANDLE;

typedef ULONG SECURITY_INFORMATION;
typedef uint32_t LCID;

#define POINTER_ALIGNMENT

enum DEVICE_RELATION_TYPE {
    BusRelations,
    EjectionRelations,
    PowerRelations,
    RemovalRelations,
    TargetDeviceRelation,
    SingleBusRelations
};

enum BUS_QUERY_ID_TYPE{
    BusQueryDeviceID = 0,
    BusQueryHardwareIDs = 1,
    BusQueryCompatibleIDs = 2,
    BusQueryInstanceID = 3,
    BusQueryDeviceSerialNumber = 4
};

enum DEVICE_TEXT_TYPE {
    DeviceTextDescription = 0,
    DeviceTextLocationInformation = 1
};

enum DEVICE_USAGE_NOTIFICATION_TYPE {
    DeviceUsageTypeUndefined,
    DeviceUsageTypePaging,
    DeviceUsageTypeHibernation,
    DeviceUsageTypeDumpFile
};

enum SYSTEM_POWER_STATE {
    PowerSystemUnspecified = 0,
    PowerSystemWorking     = 1,
    PowerSystemSleeping1   = 2,
    PowerSystemSleeping2   = 3,
    PowerSystemSleeping3   = 4,
    PowerSystemHibernate   = 5,
    PowerSystemShutdown    = 6,
    PowerSystemMaximum     = 7
};

enum POWER_STATE_TYPE {
    SystemPowerState = 0,
    DevicePowerState
};

enum DEVICE_POWER_STATE {
    PowerDeviceUnspecified = 0,
    PowerDeviceD0,
    PowerDeviceD1,
    PowerDeviceD2,
    PowerDeviceD3,
    PowerDeviceMaximum
};

enum POWER_ACTION{
    PowerActionNone = 0,
    PowerActionReserved,
    PowerActionSleep,
    PowerActionHibernate,
    PowerActionShutdown,
    PowerActionShutdownReset,
    PowerActionShutdownOff,
    PowerActionWarmEject
};

union POWER_STATE {
    SYSTEM_POWER_STATE SystemState;
    DEVICE_POWER_STATE DeviceState;
};

enum FILE_INFORMATION_CLASS {
    FileDirectoryInformation         = 1,
    FileFullDirectoryInformation,   // 2
    FileBothDirectoryInformation,   // 3
    FileBasicInformation,           // 4
    FileStandardInformation,        // 5
    FileInternalInformation,        // 6
    FileEaInformation,              // 7
    FileAccessInformation,          // 8
    FileNameInformation,            // 9
    FileRenameInformation,          // 10
    FileLinkInformation,            // 11
    FileNamesInformation,           // 12
    FileDispositionInformation,     // 13
    FilePositionInformation,        // 14
    FileFullEaInformation,          // 15
    FileModeInformation,            // 16
    FileAlignmentInformation,       // 17
    FileAllInformation,             // 18
    FileAllocationInformation,      // 19
    FileEndOfFileInformation,       // 20
    FileAlternateNameInformation,   // 21
    FileStreamInformation,          // 22
    FilePipeInformation,            // 23
    FilePipeLocalInformation,       // 24
    FilePipeRemoteInformation,      // 25
    FileMailslotQueryInformation,   // 26
    FileMailslotSetInformation,     // 27
    FileCompressionInformation,     // 28
    FileObjectIdInformation,        // 29
    FileCompletionInformation,      // 30
    FileMoveClusterInformation,     // 31
    FileQuotaInformation,           // 32
    FileReparsePointInformation,    // 33
    FileNetworkOpenInformation,     // 34
    FileAttributeTagInformation,    // 35
    FileTrackingInformation,        // 36
    FileIdBothDirectoryInformation, // 37
    FileIdFullDirectoryInformation, // 38
    FileValidDataLengthInformation, // 39
    FileShortNameInformation,       // 40
    FileMaximumInformation
};

enum FS_INFORMATION_CLASS {
    FileFsVolumeInformation       = 1,
    FileFsLabelInformation,      // 2
    FileFsSizeInformation,       // 3
    FileFsDeviceInformation,     // 4
    FileFsAttributeInformation,  // 5
    FileFsControlInformation,    // 6
    FileFsFullSizeInformation,   // 7
    FileFsObjectIdInformation,   // 8
    FileFsDriverPathInformation, // 9
    FileFsMaximumInformation
};

struct SECURITY_DESCRIPTOR32 {
    uint8_t Revision;
    uint8_t Sbz1;
    SECURITY_DESCRIPTOR_CONTROL Control;
    PSID32 Owner;
    PSID32 Group;
    PACL32 Sacl;
    PACL32 Dacl;
}__attribute__((packed));



typedef struct _FILE_OBJECT *PFILE_OBJECT;

struct IO_STACK_LOCATION {
    UCHAR MajorFunction;
    UCHAR MinorFunction;
    UCHAR Flags;
    UCHAR Control;

    union {
        struct {
            uint32_t SecurityContext;
            ULONG Options;
            USHORT POINTER_ALIGNMENT FileAttributes;
            USHORT ShareAccess;
            ULONG POINTER_ALIGNMENT EaLength;
        } Create;

        struct {
            ULONG Length;
            ULONG POINTER_ALIGNMENT Key;
            uint64_t ByteOffset;
        } Read;

        struct {
            ULONG Length;
            ULONG POINTER_ALIGNMENT Key;
            uint64_t ByteOffset;
        } Write;

        struct {
            ULONG Length;
            FILE_INFORMATION_CLASS POINTER_ALIGNMENT FileInformationClass;
        } QueryFile;

        struct {
            ULONG Length;
            FILE_INFORMATION_CLASS POINTER_ALIGNMENT FileInformationClass;
            uint32_t FileObject;
            union {
                struct {
                    BOOLEAN ReplaceIfExists;
                    BOOLEAN AdvanceOnly;
                };
                ULONG ClusterCount;
                HANDLE DeleteHandle;
            };
        } SetFile;

        struct {
            ULONG Length;
            FS_INFORMATION_CLASS POINTER_ALIGNMENT FsInformationClass;
        } QueryVolume;


        struct {
            ULONG OutputBufferLength;
            ULONG POINTER_ALIGNMENT InputBufferLength;
            ULONG POINTER_ALIGNMENT IoControlCode;
            uint32_t Type3InputBuffer;
        } DeviceIoControl;


        struct {
            SECURITY_INFORMATION SecurityInformation;
            ULONG POINTER_ALIGNMENT Length;
        } QuerySecurity;


        struct {
            SECURITY_INFORMATION SecurityInformation;
            uint32_t SecurityDescriptor;
        } SetSecurity;


        struct {
            uint32_t Vpb;
            uint32_t DeviceObject;
        } MountVolume;


        struct {
            uint32_t Vpb;
            uint32_t DeviceObject;
        } VerifyVolume;

        struct {
            uint32_t Srb;
        } Scsi;

        struct {
            DEVICE_RELATION_TYPE Type;
        } QueryDeviceRelations;

        struct {
            uint32_t InterfaceType;
            USHORT Size;
            USHORT Version;
            uint32_t Interface;
            uint32_t InterfaceSpecificData;
        } QueryInterface;

        struct {
            uint32_t Capabilities;
        } DeviceCapabilities;

        struct {
            uint32_t IoResourceRequirementList;
        } FilterResourceRequirements;

        struct {
            ULONG WhichSpace;
            uint32_t Buffer;
            ULONG Offset;
            ULONG POINTER_ALIGNMENT Length;
        } ReadWriteConfig;

        struct {
            BOOLEAN Lock;
        } SetLock;

        struct {
            BUS_QUERY_ID_TYPE IdType;
        } QueryId;

        struct {
            DEVICE_TEXT_TYPE DeviceTextType;
            LCID POINTER_ALIGNMENT LocaleId;
        } QueryDeviceText;

        struct {
            BOOLEAN InPath;
            BOOLEAN Reserved[3];
            DEVICE_USAGE_NOTIFICATION_TYPE POINTER_ALIGNMENT Type;
        } UsageNotification;

        struct {
            SYSTEM_POWER_STATE PowerState;
        } WaitWake;

        struct {
            uint32_t PowerSequence;
        } PowerSequence;

        struct {
            ULONG SystemContext;
            POWER_STATE_TYPE POINTER_ALIGNMENT Type;
            POWER_STATE POINTER_ALIGNMENT State;
            POWER_ACTION POINTER_ALIGNMENT ShutdownType;
        } Power;

        struct {
            uint32_t AllocatedResources;
            uint32_t AllocatedResourcesTranslated;
        } StartDevice;

        struct {
            uint32_t ProviderId;
            uint32_t DataPath;
            ULONG BufferSize;
            uint32_t Buffer;
        } WMI;

        struct {
            uint32_t Argument1;
            uint32_t Argument2;
            uint32_t Argument3;
            uint32_t Argument4;
        } Others;

    } Parameters;

    uint32_t DeviceObject;

    uint32_t FileObject; //FILE_OBJECT

    uint32_t CompletionRoutine;

    uint32_t Context;

}__attribute__((packed));

struct KAPC32 {
    uint16_t Type;
    uint16_t Size;
    uint32_t Spare0;
    uint32_t Thread;
    LIST_ENTRY32 ApcListEntry;
    uint32_t KernelRoutine;
    uint32_t RundownRoutine;
    uint32_t NormalRoutine;
    uint32_t NormalContext;

    uint32_t SystemArgument1;
    uint32_t SystemArgument2;
    uint8_t ApcStateIndex;
    KPROCESSOR_MODE ApcMode;
    BOOLEAN Inserted;
};

struct KDEVICE_QUEUE_ENTRY32 {
    LIST_ENTRY32 DeviceListEntry;
    uint32_t SortKey;
    BOOLEAN Inserted;
};

struct IO_STATUS_BLOCK32 {
    union {
        uint32_t Status;
        uint32_t Pointer;
    };

    uint32_t Information;
};

struct IRP {
    uint16_t Type;
    uint16_t Size;
    uint32_t MdlAddress;
    uint32_t Flags;

    union {
        uint32_t MasterIrp;
        int32_t IrpCount;
        uint32_t SystemBuffer;
    } AssociatedIrp;

    LIST_ENTRY32 ThreadListEntry;
    IO_STATUS_BLOCK32 IoStatus;
    int8_t RequestorMode;
    uint8_t PendingReturned;
    int8_t StackCount;
    int8_t CurrentLocation;
    uint8_t Cancel;
    uint8_t CancelIrql;
    int8_t ApcEnvironment;
    uint8_t AllocationFlags;

    uint32_t UserIosb;
    uint32_t UserEvent;
    union {
        struct {
            uint32_t UserApcRoutine;
            uint32_t UserApcContext;
        } AsynchronousParameters;
        uint64_t AllocationSize;
    } Overlay;

    uint32_t CancelRoutine;
    uint32_t UserBuffer;

    union {
        struct {
            union {
                KDEVICE_QUEUE_ENTRY32 DeviceQueueEntry;
                struct {
                    uint32_t DriverContext[4];
                } ;
            } ;

            uint32_t Thread;
            uint32_t AuxiliaryBuffer;

            struct {
                LIST_ENTRY32 ListEntry;
                union {
                    uint32_t CurrentStackLocation; //struct IO_STACK_LOCATION *
                    uint32_t PacketType;
                };
            };
            uint32_t OriginalFileObject;
        } Overlay;

        KAPC32 Apc;
        uint32_t CompletionKey;

    } Tail;

};

static const uint32_t  IRP_MJ_CREATE                     = 0x00;
static const uint32_t  IRP_MJ_CREATE_NAMED_PIPE          = 0x01;
static const uint32_t  IRP_MJ_CLOSE                      = 0x02;
static const uint32_t  IRP_MJ_READ                       = 0x03;
static const uint32_t  IRP_MJ_WRITE                      = 0x04;
static const uint32_t  IRP_MJ_QUERY_INFORMATION          = 0x05;
static const uint32_t  IRP_MJ_SET_INFORMATION            = 0x06;
static const uint32_t  IRP_MJ_QUERY_EA                   = 0x07;
static const uint32_t  IRP_MJ_SET_EA                     = 0x08;
static const uint32_t  IRP_MJ_FLUSH_BUFFERS              = 0x09;
static const uint32_t  IRP_MJ_QUERY_VOLUME_INFORMATION   = 0x0a;
static const uint32_t  IRP_MJ_SET_VOLUME_INFORMATION     = 0x0b;
static const uint32_t  IRP_MJ_DIRECTORY_CONTROL          = 0x0c;
static const uint32_t  IRP_MJ_FILE_SYSTEM_CONTROL        = 0x0d;
static const uint32_t  IRP_MJ_DEVICE_CONTROL             = 0x0e;
static const uint32_t  IRP_MJ_INTERNAL_DEVICE_CONTROL    = 0x0f;
static const uint32_t  IRP_MJ_SCSI                       = 0x0f;
static const uint32_t  IRP_MJ_SHUTDOWN                   = 0x10;
static const uint32_t  IRP_MJ_LOCK_CONTROL               = 0x11;
static const uint32_t  IRP_MJ_CLEANUP                    = 0x12;
static const uint32_t  IRP_MJ_CREATE_MAILSLOT            = 0x13;
static const uint32_t  IRP_MJ_QUERY_SECURITY             = 0x14;
static const uint32_t  IRP_MJ_SET_SECURITY               = 0x15;
static const uint32_t  IRP_MJ_POWER                      = 0x16;
static const uint32_t  IRP_MJ_SYSTEM_CONTROL             = 0x17;
static const uint32_t  IRP_MJ_DEVICE_CHANGE              = 0x18;
static const uint32_t  IRP_MJ_QUERY_QUOTA                = 0x19;
static const uint32_t  IRP_MJ_SET_QUOTA                  = 0x1a;
static const uint32_t  IRP_MJ_PNP                        = 0x1b;
static const uint32_t  IRP_MJ_PNP_POWER                  = 0x1b;
static const uint32_t  IRP_MJ_MAXIMUM_FUNCTION           = 0x1b;



} //namespace windows
} //namespace s2e

/****************************************************************/
/****************************************************************/
/****************************************************************/

#include <s2e/Plugins/ExecutableImage.h>
#include <s2e/Plugins/ModuleDescriptor.h>

namespace s2e
{
namespace plugins
{

class WindowsImage:IExecutableImage
{

private:
  s2e::windows::IMAGE_DOS_HEADER DosHeader;
  s2e::windows::IMAGE_NT_HEADERS NtHeader;

  uint64_t m_Base;
  uint64_t m_ImageBase;
  uint64_t m_EntryPoint;
  unsigned m_ImageSize;

  /* Stores the relative addresses of all exported functions */
  Exports m_Exports;
  bool m_ExportsInited;

  /* We assume for now that there are no name collisions between
   * function with the same name in different libraries */
  Imports m_Imports;
  bool m_ImportsInited;

  ModuleSections m_Sections;
  bool m_sectionsInited;

  int InitImports(S2EExecutionState *s);
  int InitExports(S2EExecutionState *s);
  bool InitSections(S2EExecutionState *s);

  static bool IsValidString(const char *str);
public:
  WindowsImage(S2EExecutionState *s, uint64_t Base);

  virtual uint64_t GetBase() const {
    return m_Base;
  }

  virtual uint64_t GetImageBase() const {
    return m_ImageBase;
  }

  virtual uint64_t GetImageSize() const {
    return m_ImageSize;
  }

  virtual uint64_t GetEntryPoint() const {
    return m_EntryPoint;
  }

  virtual uint64_t GetRoundedImageSize() const;
  virtual const Exports& GetExports(S2EExecutionState *s);
  virtual const Imports& GetImports(S2EExecutionState *s);
  virtual void DumpInfo(std::ostream &os) const;
  virtual const ModuleSections &GetSections(S2EExecutionState *s);
};

/*
ntdll!_KPRCB
   +0x000 MinorVersion     : Uint2B
   +0x002 MajorVersion     : Uint2B
   +0x004 CurrentThread    : Ptr32 _KTHREAD
   +0x008 NextThread       : Ptr32 _KTHREAD
   +0x00c IdleThread       : Ptr32 _KTHREAD
   +0x010 Number           : Char
   +0x011 Reserved         : Char
   +0x012 BuildType        : Uint2B
   +0x014 SetMember        : Uint4B
   +0x018 CpuType          : Char
   +0x019 CpuID            : Char
   +0x01a CpuStep          : Uint2B
   +0x01c ProcessorState   : _KPROCESSOR_STATE
   +0x33c KernelReserved   : [16] Uint4B
   +0x37c HalReserved      : [16] Uint4B
   +0x3bc PrcbPad0         : [92] UChar
   +0x418 LockQueue        : [16] _KSPIN_LOCK_QUEUE
   +0x498 PrcbPad1         : [8] UChar
   +0x4a0 NpxThread        : Ptr32 _KTHREAD
   +0x4a4 InterruptCount   : Uint4B
   +0x4a8 KernelTime       : Uint4B
   +0x4ac UserTime         : Uint4B
   +0x4b0 DpcTime          : Uint4B
   +0x4b4 DebugDpcTime     : Uint4B
   +0x4b8 InterruptTime    : Uint4B
   +0x4bc AdjustDpcThreshold : Uint4B
   +0x4c0 PageColor        : Uint4B
   +0x4c4 SkipTick         : Uint4B
   +0x4c8 MultiThreadSetBusy : UChar
   +0x4c9 Spare2           : [3] UChar
   +0x4cc ParentNode       : Ptr32 _KNODE
   +0x4d0 MultiThreadProcessorSet : Uint4B
   +0x4d4 MultiThreadSetMaster : Ptr32 _KPRCB
   +0x4d8 ThreadStartCount : [2] Uint4B
   +0x4e0 CcFastReadNoWait : Uint4B
   +0x4e4 CcFastReadWait   : Uint4B
   +0x4e8 CcFastReadNotPossible : Uint4B
   +0x4ec CcCopyReadNoWait : Uint4B
   +0x4f0 CcCopyReadWait   : Uint4B
   +0x4f4 CcCopyReadNoWaitMiss : Uint4B
   +0x4f8 KeAlignmentFixupCount : Uint4B
   +0x4fc KeContextSwitches : Uint4B
   +0x500 KeDcacheFlushCount : Uint4B
   +0x504 KeExceptionDispatchCount : Uint4B
   +0x508 KeFirstLevelTbFills : Uint4B
   +0x50c KeFloatingEmulationCount : Uint4B
   +0x510 KeIcacheFlushCount : Uint4B
   +0x514 KeSecondLevelTbFills : Uint4B
   +0x518 KeSystemCalls    : Uint4B
   +0x51c SpareCounter0    : [1] Uint4B
   +0x520 PPLookasideList  : [16] _PP_LOOKASIDE_LIST
   +0x5a0 PPNPagedLookasideList : [32] _PP_LOOKASIDE_LIST
   +0x6a0 PPPagedLookasideList : [32] _PP_LOOKASIDE_LIST
   +0x7a0 PacketBarrier    : Uint4B
   +0x7a4 ReverseStall     : Uint4B
   +0x7a8 IpiFrame         : Ptr32 Void
   +0x7ac PrcbPad2         : [52] UChar
   +0x7e0 CurrentPacket    : [3] Ptr32 Void
   +0x7ec TargetSet        : Uint4B
   +0x7f0 WorkerRoutine    : Ptr32     void
   +0x7f4 IpiFrozen        : Uint4B
   +0x7f8 PrcbPad3         : [40] UChar
   +0x820 RequestSummary   : Uint4B
   +0x824 SignalDone       : Ptr32 _KPRCB
   +0x828 PrcbPad4         : [56] UChar
   +0x860 DpcListHead      : _LIST_ENTRY
   +0x868 DpcStack         : Ptr32 Void
   +0x86c DpcCount         : Uint4B
   +0x870 DpcQueueDepth    : Uint4B
   +0x874 DpcRoutineActive : Uint4B
   +0x878 DpcInterruptRequested : Uint4B
   +0x87c DpcLastCount     : Uint4B
   +0x880 DpcRequestRate   : Uint4B
   +0x884 MaximumDpcQueueDepth : Uint4B
   +0x888 MinimumDpcRate   : Uint4B
   +0x88c QuantumEnd       : Uint4B
   +0x890 PrcbPad5         : [16] UChar
   +0x8a0 DpcLock          : Uint4B
   +0x8a4 PrcbPad6         : [28] UChar
   +0x8c0 CallDpc          : _KDPC
   +0x8e0 ChainedInterruptList : Ptr32 Void
   +0x8e4 LookasideIrpFloat : Int4B
   +0x8e8 SpareFields0     : [6] Uint4B
   +0x900 VendorString     : [13] UChar
   +0x90d InitialApicId    : UChar
   +0x90e LogicalProcessorsPerPhysicalProcessor : UChar
   +0x910 MHz              : Uint4B
   +0x914 FeatureBits      : Uint4B
   +0x918 UpdateSignature  : _LARGE_INTEGER
   +0x920 NpxSaveArea      : _FX_SAVE_AREA
   +0xb30 PowerState       : _PROCESSOR_POWER_STATE
*/
}

}
#endif
