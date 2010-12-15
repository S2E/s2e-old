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
 * Main authors: Vitaly Chipounov, Volodymyr Kuznetsov.
 * All S2E contributors are listed in the S2E-AUTHORS file.
 *
 */

#ifndef S2ETOOLS_BFDINTERFACE_H
#define S2ETOOLS_BFDINTERFACE_H

extern "C" {
#include <bfd.h>
}

#include <string>
#include <map>
#include <set>
#include <inttypes.h>

#include "ExecutableFile.h"
#include "llvm/Support/MemoryBuffer.h"

namespace s2etools
{

class Binary;

struct BFDSection
{
    uint64_t start, size;

    bool operator < (const BFDSection &s) const {
        return start + size <= s.start;
    }
};

class BFDInterface : public ExecutableFile
{
public:
    typedef std::pair<std::string, std::string> FunctionDescriptor;
    typedef std::multimap<uint64_t, FunctionDescriptor> Imports;
    typedef std::map<BFDSection, asection *> Sections;

private:

    typedef std::set<uint64_t> AddressSet;

    static bool s_bfdInited;
    bfd *m_bfd;
    asymbol **m_symbolTable;
    long m_symbolCount;

    std::string m_moduleName;
    Sections m_sections;
    AddressSet m_invalidAddresses;

    uint64_t m_imageBase;
    bool m_requireSymbols;
    llvm::MemoryBuffer *m_file;
    Binary *m_binary;

    //This for copy-on-write, when we need to write stuff to the BFD
    std::map<uint64_t, uint8_t> m_cowBuffer;

    static void initSections(bfd *abfd, asection *sect, void *obj);

    bool initPeImports();
    asection *getSection(uint64_t va, unsigned size) const;

public:
    BFDInterface(const std::string &fileName);
    BFDInterface(const std::string &fileName, bool requireSymbols);
    virtual ~BFDInterface();

    //Autodetects the bfd format
    bool initialize();

    //Specifies the bfd format to use
    bool initialize(const std::string &format);

    bool getInfo(uint64_t addr, std::string &source, uint64_t &line, std::string &function);
    bool inited() const {
        return m_bfd != NULL;
    }

    virtual bool getModuleName(std::string &name ) const;
    virtual uint64_t getImageBase() const;
    virtual uint64_t getImageSize() const;

    //Gets the address of the first executable instruction of the file
    uint64_t getEntryPoint() const;

    //Read the contents at virtual address va
    bool read(uint64_t va, void *dest, unsigned size) const;

    bool write(uint64_t va, void *source, unsigned size);

    const bool getImports(Imports &imports) const;
    const bool getRelocations(RelocationEntries &relocs) const;

    //Returns whether the supplied address is in an executable section
    bool isCode(uint64_t va) const;
    bool isData(uint64_t va) const;
    int getSectionFlags(uint64_t va) const;

    const Sections &getSections() const {
        return m_sections;
    }

    asymbol **getSymbols() const {
        return m_symbolTable;
    }

    long getSymbolCount() const {
        return m_symbolCount;
    }

    llvm::MemoryBuffer *getFile() const {
        return m_file;
    }

};

}

#endif
