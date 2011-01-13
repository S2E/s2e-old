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

namespace s2etools {

PeReader::PeReader(BFDInterface *bfd):Binary(bfd)
{
    m_file = getBfd()->getFile();
    assert(isValid(m_file));
    initialize();
    resolveImports();
}

bool PeReader::isValid(llvm::MemoryBuffer *file)
{
    const windows::IMAGE_DOS_HEADER *dosHeader;
    const windows::IMAGE_NT_HEADERS *ntHeader;
    const uint8_t *start = (uint8_t*)file->getBufferStart();

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

    return true;
}

bool PeReader::initialize()
{
    const uint8_t *start = (uint8_t*)m_file->getBufferStart();
    m_dosHeader = *(windows::IMAGE_DOS_HEADER*)start;
    m_ntHeader = *(windows::IMAGE_NT_HEADERS *)(start + m_dosHeader.e_lfanew);
    return true;
}


bool PeReader::resolveImports()
{
    const uint8_t *start = (uint8_t*)m_file->getBufferStart();

    const windows::IMAGE_DATA_DIRECTORY *importDir;
    importDir =  &m_ntHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDir->Size || !importDir->Size) {
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

        while(importNameTable->u1.AddressOfData) {
            const char *functionName = NULL;

            if (importNameTable->u1.AddressOfData & IMAGE_ORDINAL_FLAG) {
                uint32_t tmp = importNameTable->u1.AddressOfData & ~0xFFFF;
                tmp &= ~IMAGE_ORDINAL_FLAG;
                if (!tmp) {
                    std::cerr << "No support for import by ordinals" << std::endl;
                    continue;
                }
            }else {
                windows::IMAGE_IMPORT_BY_NAME *byName = (windows::IMAGE_IMPORT_BY_NAME*)(start + importNameTable->u1.AddressOfData);
                functionName = (const char*)&byName->Name;
            }

            //XXX: Need to allocate the function here
            assert(false && "Address allocation for import binding not implemented yet");
            //importAddressTable->u1.Function = ...; Write to the bfd the actual address

            m_imports.insert(std::make_pair(importAddressTable->u1.Function, std::make_pair(moduleName, functionName)));

            importAddressTable++;
            importNameTable++;
        }
    }

    return true;

}



}
