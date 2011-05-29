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

#include <cassert>
#include <iostream>
#include <algorithm>
#include "Pe.h"
#include "BFDInterface.h"


using namespace s2etools::windows;

namespace s2etools {

LogKey PeReader::TAG = LogKey("PeReader");

PeReader::PeReader(BFDInterface *bfd):Binary(bfd)
{
    m_file = getBfd()->getFile();
    assert(isValid(m_file));
    initialize();
    resolveImports();
    processesRelocations();
}

bool PeReader::isValid(MemoryFile *file)
{
    const windows::IMAGE_DOS_HEADER *dosHeader;
    const windows::IMAGE_NT_HEADERS *ntHeader;
    const uint8_t *start = (uint8_t*)file->getBuffer();

    dosHeader = (windows::IMAGE_DOS_HEADER*)start;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)  {
        return false;
    }

    ntHeader = (windows::IMAGE_NT_HEADERS *)(start + dosHeader->e_lfanew);

    if (ntHeader->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    if (ntHeader->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        return false;
    }

    if (ntHeader->OptionalHeader.FileAlignment != ntHeader->OptionalHeader.SectionAlignment) {
        LOGERROR("The binary must have the same on-disk and in-memory alignment!" << std::endl);
        return false;
    }

    return true;
}

bool PeReader::initialize()
{
    const uint8_t *start = (uint8_t*)m_file->getBuffer();
    m_dosHeader = *(windows::IMAGE_DOS_HEADER*)start;
    m_ntHeader = *(windows::IMAGE_NT_HEADERS *)(start + m_dosHeader.e_lfanew);
    m_importedAddressPtr = getBfd()->getImageBase() + getBfd()->getImageSize();
    return true;
}

//XXX: file alignment needs to be taken into account
bool PeReader::resolveImports()
{
    const uint8_t *start = (uint8_t*)m_file->getBuffer();

    const windows::IMAGE_DATA_DIRECTORY *importDir;
    importDir =  &m_ntHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDir->Size || !importDir->VirtualAddress) {
        return false;
    }

    const windows::IMAGE_IMPORT_DESCRIPTOR *importDescriptors;
    unsigned importDescCount;

    importDescCount = importDir->Size / sizeof(windows::IMAGE_IMPORT_DESCRIPTOR);
    importDescriptors = (windows::IMAGE_IMPORT_DESCRIPTOR *)(
            start + importDir->VirtualAddress);

    for (unsigned int i=0; importDescriptors[i].Characteristics && i<importDescCount; i++) {
        const char *mn = (const char *)(start + importDescriptors[i].Name);
        std::string moduleName = mn;
        std::transform(moduleName.begin(), moduleName.end(), moduleName.begin(), ::tolower);

        windows::IMAGE_THUNK_DATA32 *importAddressTable =
                (windows::IMAGE_THUNK_DATA32 *)(start + importDescriptors[i].FirstThunk);

        windows::IMAGE_THUNK_DATA32 *importNameTable =
                (windows::IMAGE_THUNK_DATA32 *)(start + importDescriptors[i].OriginalFirstThunk);

        StartSizePair iat(importDescriptors[i].FirstThunk + m_ntHeader.OptionalHeader.ImageBase, 0);

        while(importNameTable->u1.AddressOfData) {
            iat.size+=sizeof(windows::IMAGE_THUNK_DATA32);
            const char *functionName = NULL;

            if (importNameTable->u1.AddressOfData & IMAGE_ORDINAL_FLAG) {
                uint32_t tmp = importNameTable->u1.AddressOfData & ~0xFFFF;
                tmp &= ~IMAGE_ORDINAL_FLAG;
                if (!tmp) {
                    LOGERROR("No support for import by ordinals" << std::endl);
                    continue;
                }
            }else {
                windows::IMAGE_IMPORT_BY_NAME *byName = (windows::IMAGE_IMPORT_BY_NAME*)(start + importNameTable->u1.AddressOfData);
                functionName = (const char*)&byName->Name;
            }

            //XXX: Need to allocate the function here
            //assert(false && "Address allocation for import binding not implemented yet");
            //importAddressTable->u1.Function = ...; Write to the bfd the actual address
            importAddressTable->u1.Function = m_importedAddressPtr;
            m_importedAddressPtr += 0x1000;

            m_imports.insert(std::make_pair(importAddressTable->u1.Function, std::make_pair(moduleName, functionName)));

            importAddressTable++;
            importNameTable++;
        }
        m_iat.insert(iat);
    }

    return true;

}

bool PeReader::processesRelocations()
{
    const IMAGE_DATA_DIRECTORY *relocsDir;
    relocsDir = &m_ntHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (!relocsDir->Size || !relocsDir->VirtualAddress) {
        return false;
    }

    const uint8_t *start = (uint8_t*)m_file->getBuffer();

    IMAGE_BASE_RELOCATION *relocs = (IMAGE_BASE_RELOCATION*)(start + relocsDir->VirtualAddress);

    uint32_t loadBase = m_ntHeader.OptionalHeader.ImageBase;
    uint32_t sectionOffset = 0;

    while (sectionOffset < relocsDir->Size) {
        unsigned relocCount = (relocs->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
        uint16_t *relocEntries = (uint16_t*)((uint8_t*)relocs + sizeof(IMAGE_BASE_RELOCATION));
        LOGDEBUG("RelocChunk: 0x" << std::hex << relocs->VirtualAddress + loadBase << std::endl);
        for (unsigned i=0; i<relocCount; ++i) {
            unsigned type = relocEntries[i] >> 12;
            unsigned offset = relocEntries[i] & 0xFFF;

            if (relocEntries[i] == 0) {
                break;
            }

            assert(type == IMAGE_REL_BASED_HIGHLOW && "We support only IMAGE_REL_BASED_HIGHLOW");

            RelocationEntry re;
            re.va = relocs->VirtualAddress + loadBase + offset;
            re.size = sizeof(uint32_t);

            if (!read(re.va, &re.originalValue, re.size)) {
                LOGERROR("Could not read data at address 0x" << std::hex << re.va << std::endl);
                continue;
            }
            LOGDEBUG(std::hex << "  RelocEntry: " << type << " Offset: " << offset << " OrigValue=0x" << re.originalValue << std::endl);


            re.targetValue = re.originalValue;

            //XXX: Retrieve the symbol name, if available.
            //Need to parse the import table
            re.symbolName = "";
            re.symbolBase = 0;
            re.symbolIndex = 0;
            m_relocations[re.va] = re;
        }
        sectionOffset += relocs->SizeOfBlock;
        relocs = (IMAGE_BASE_RELOCATION *)(((uint8_t*)relocs) + relocs->SizeOfBlock);
    }
    return true;
}

//XXX: Fix for 64-bit binaries
uint64_t PeReader::readAddressFromImportTable(uint64_t va) const
{
    //Check it's indeed in the import section
    StartSizePair p(va, 4);
    if (m_iat.find(p) == m_iat.end()) {
        return 0;
    }

    uint32_t offset = va - m_ntHeader.OptionalHeader.ImageBase;
    uint32_t *value = (uint32_t*)(m_file->getBuffer() + offset);

    return *value;
}

}
