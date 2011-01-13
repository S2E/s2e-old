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
#include <string.h>

#include "Macho.h"
#include "BFDInterface.h"

using namespace s2etools::macos;
using namespace llvm;

namespace s2etools
{

MachoReader::MachoReader(BFDInterface *bfd):Binary(bfd)
 {
     m_file = getBfd()->getFile();
     memset(&m_dynSymCmd, 0, sizeof(m_dynSymCmd));
     assert(isValid(m_file));
     m_nextAvailableAddress = 0;
     initialize();
 }

bool MachoReader::isValid(llvm::MemoryBuffer *file)
{
    const macos::macho_header *header = (macos::macho_header*)file->getBufferStart();
    if (header->magic != MACHO_SIGNATURE) {
        return false;
    }

    //Check the right architecture.
    //We do not support anything but i386 for now.
    if (header->cputype != CPU_TYPE_I386) {
        return false;
    }

    return true;
}

bool MachoReader::initialize()
{
    if (!parse()) {
        return false;
    }

    resolveImports();
    resolveRelocations();
    return true;
}

bool MachoReader::parse()
{
    bool dynsymInited = false;
    const uint8_t *start = (uint8_t*)m_file->getBufferStart();
    macho_header *header = (macho_header*)start;
    uint32_t curOffset = sizeof(macos::macho_header);

    for (uint32_t i=0; i<header->ncmds; ++i) {
        const macho_load_command *cmd =
                (macho_load_command*)(start + curOffset);

        switch(cmd->cmd) {
            case LC_DYSYMTAB:
                assert(!dynsymInited && "Multiple LC_DYSYMTAB commands. Strange.");
                m_dynSymCmd = *(macho_dysymtab_command*)(start + curOffset);
                dynsymInited = true;
                break;

            case LC_SEGMENT:
                {
                    const macho_segment_command *segment =
                                (macos::macho_segment_command*)(start + curOffset);

                    const macos::macho_section *section =
                                        (macos::macho_section *)(start + curOffset + sizeof(*segment));
                    for (unsigned secNum=0; secNum<segment->nsects; ++secNum) {
                        m_sections.push_back(section[secNum]);
                    }
                }
                break;

            default:
                break;
        }
        curOffset += cmd->cmdsize;
    }

    return true;
}

//Resolves external relocations for now
bool MachoReader::resolveRelocations()
{
    assert(m_nextAvailableAddress);

    if (!m_dynSymCmd.cmdsize) {
        std::cerr << "MachoReader: no LC_DYSYMTAB in executable. Cannot resolve external relocations." << std::endl;
        return false;
    }

    asymbol **symbolTable = getBfd()->getSymbols();

    macos::relocation_info *extRelocs =
            (macos::relocation_info*)(m_file->getBufferStart() + m_dynSymCmd.extreloff);
    uint32_t extRelocsCount = m_dynSymCmd.nextrel;

    //Patch all the references
    for (uint32_t i=0; i<extRelocsCount; ++i) {
        if (!extRelocs[i].r_extern) {
            continue;
        }

        //External relocation symbols have already been allocated
        //when scanning the import table.
        std::string symbName = symbolTable[extRelocs[i].r_symbolnum]->name;
        assert(m_importsByName.find(symbName) != m_importsByName.end());


        uint32_t relocationPointVa = extRelocs[i].r_address;
        uint32_t addressOfSymbol = (uint32_t)m_importsByName[symbName];
        uint32_t origValue;

        bool b = getBfd()->read(relocationPointVa, &origValue, sizeof(origValue));
        assert(b);

        RelocationEntry relEntry;
        relEntry.originalValue = origValue;
        relEntry.size = 1 << extRelocs[i].r_length;
        relEntry.symbolBase = addressOfSymbol;
        relEntry.targetValue = addressOfSymbol + origValue;
        relEntry.va = relocationPointVa;
        relEntry.symbolIndex = extRelocs[i].r_symbolnum;
        relEntry.symbolName = symbolTable[relEntry.symbolIndex]->name;

        b = getBfd()->write(relocationPointVa, &relEntry.targetValue, sizeof(relocationPointVa));
        assert(b);
        
        std::cout << "extRelocs[i].r_type=" << extRelocs[i].r_type <<
                " origValue=0x" << std::hex << relEntry.originalValue <<
                std::endl;


        m_relocations[relEntry.va] = relEntry;
    }

    return true;
}

bool MachoReader::resolveImports()
{
    if (!m_dynSymCmd.cmdsize) {
        std::cerr << "MachoReader: no LC_DYSYMTAB in executable. Cannot resolve imports." << std::endl;
        return false;
    }

    //Compute the first virtual address available.
    //We'll put the fictive imported functions there.
    m_nextAvailableAddress = 0;
    Sections::iterator secit;
    for(secit = m_sections.begin(); secit != m_sections.end(); ++secit) {
        uint64_t va = (*secit).addr;
        uint64_t size = (*secit).size;
        if (va + size > m_nextAvailableAddress) {
            m_nextAvailableAddress = va + size;
        }
    }


    std::map<std::string, uint64_t> symbToAddress;
    std::vector<std::string> undefinedSymbols;

    uint8_t *start = (uint8_t*)m_file->getBufferStart();
    asymbol **symbols = getBfd()->getSymbols();

    //Find a undefined symbols, allocate an address for them
    for (long i = 0; i<getBfd()->getSymbolCount(); ++i) {
        if (!bfd_is_und_section(symbols[i]->section)) {
            continue;
        }

        undefinedSymbols.push_back(symbols[i]->name);

        std::cout << "Import " << symbols[i]->name << " at address 0x" << std::hex <<  m_nextAvailableAddress << std::endl;
        m_imports.insert(std::make_pair(m_nextAvailableAddress, std::make_pair("libc", symbols[i]->name)));
        m_importsByName[symbols[i]->name] = m_nextAvailableAddress;
        symbToAddress[symbols[i]->name] = m_nextAvailableAddress;
        m_nextAvailableAddress += 0x1000;
    }

    asymbol **symbolTable = getBfd()->getSymbols();

    //Go through the sections and patch the jump tables
    Sections::iterator secIt;
    for(secIt = m_sections.begin(); secIt != m_sections.end(); ++secIt) {
        const macho_section &section = *secIt;
        if (!(section.flags & S_SYMBOL_STUBS)) {
            //XXX: Check that there are no other section types that need to be patched
            continue;
        }

        uint32_t indirectIndex = section.reserved1;
        uint32_t stubSize = section.reserved2;
        uint32_t indexCount = section.size / stubSize;

        for (unsigned i=0; i<indexCount; ++i) {
            //Compute the index of into the indirect table
            uint32_t rawIdx = i + indirectIndex;
            assert (rawIdx < m_dynSymCmd.nindirectsyms);

            //The computed position in the indirect table stores the
            //index of the symbol in the symbol table
            uint32_t symbolIndex = ((uint32_t*)(start + m_dynSymCmd.indirectsymoff))[rawIdx];

            std::cout << "Patching " << symbolTable[symbolIndex]->name << std::endl;
            //Get the address of the symbol
            uint32_t addrToWrite = symbToAddress[symbolTable[symbolIndex]->name];
            assert(addrToWrite);
            uint32_t instrAddr = (*secIt).addr + i * stubSize;

            //Patch the binary with an indirect jump to the new location
            //XXX: this is x86 only
            uint8_t jmpOpCode = 0xe9;
            bool b = getBfd()->write(instrAddr, &jmpOpCode, sizeof(jmpOpCode));
            assert(b);
            addrToWrite = addrToWrite - (instrAddr + 5);
            b = getBfd()->write(instrAddr + 1, &addrToWrite, sizeof(addrToWrite));
            assert(b);
        }
    }

    return true;
}


}
