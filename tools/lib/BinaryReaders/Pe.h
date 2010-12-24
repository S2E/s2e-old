#ifndef S2ETOOLS_PE_H
#define S2ETOOLS_PE_H

#include <inttypes.h>
#include <cassert>
#include "Binary.h"

namespace s2etools {
namespace windows {

#define IMAGE_DOS_SIGNATURE                 0x5A4D      // MZ
#define IMAGE_NT_SIGNATURE                  0x00004550  // PE00

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

#define IMAGE_ORDINAL_FLAG  0x80000000

struct IMAGE_DOS_HEADER {      // DOS .EXE header
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
  }__attribute__ ((packed));

struct IMAGE_FILE_HEADER {	//20 bytes
    uint16_t    Machine;
    uint16_t    NumberOfSections;
    uint32_t   TimeDateStamp;
    uint32_t   PointerToSymbolTable;
    uint32_t   NumberOfSymbols;
    uint16_t    SizeOfOptionalHeader;
    uint16_t    Characteristics;
} __attribute__ ((packed));

#define IMAGE_FILE_MACHINE_I386 0x014c

struct IMAGE_DATA_DIRECTORY {
    uint32_t   VirtualAddress;
    uint32_t   Size;
} __attribute__ ((packed));

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES    16

struct IMAGE_OPTIONAL_HEADER {			//96 + ... bytes
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
} __attribute__ ((packed));

struct IMAGE_NT_HEADERS {				//
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER OptionalHeader;
} __attribute__ ((packed));


struct IMAGE_THUNK_DATA32 {
    union {
        uint32_t ForwarderString; //PBYTE
        uint32_t Function; //Puint32_t
        uint32_t Ordinal;
        uint32_t AddressOfData; //IMAGE_IMPORT_BY_NAME  *
    } u1;
} __attribute__ ((packed));

struct IMAGE_IMPORT_BY_NAME {
    uint16_t Hint;
    uint8_t  Name[1];
}__attribute__ ((packed));

struct IMAGE_IMPORT_DESCRIPTOR {
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
} __attribute__ ((packed));


}


class PeReader: public Binary {
private:
    Imports m_imports;
    llvm::MemoryBuffer *m_file;

    windows::IMAGE_DOS_HEADER m_dosHeader;
    windows::IMAGE_NT_HEADERS m_ntHeader;


    bool initialize();
    bool resolveImports();

public:
    PeReader(BFDInterface *bfd);

    static bool isValid(llvm::MemoryBuffer *file);

    virtual const Imports &getImports() const {
        return m_imports;
    }

    virtual const RelocationEntries &getRelocations() const {
        assert(false && "Not implemented");
    }

};

}

#endif
