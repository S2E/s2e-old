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

#include "BFDInterface.h"

#include <stdlib.h>
#include <cassert>

#include <iostream>

namespace s2etools
{

bool BFDInterface::s_bfdInited = false;

BFDInterface::BFDInterface(const std::string &fileName):ExecutableFile(fileName)
{
    m_bfd = NULL;
    m_symbolTable = NULL;
    //Fail loading if the image has no symbols
    m_requireSymbols = true;
}

BFDInterface::BFDInterface(const std::string &fileName, bool requireSymbols):ExecutableFile(fileName)
{
    m_bfd = NULL;
    m_symbolTable = NULL;
    m_requireSymbols = requireSymbols;
}

BFDInterface::~BFDInterface()
{
    if (m_bfd) {
        free(m_symbolTable);
        bfd_close(m_bfd);
    }
}

void BFDInterface::initSections(bfd *abfd, asection *sect, void *obj)
{
    BFDInterface *bfdptr = (BFDInterface*)obj;

    BFDSection s;
    s.start = sect->vma;
    s.size = sect->size;

    bfdptr->m_sections[s] = sect;
}

bool BFDInterface::initialize()
{
    if (!s_bfdInited) {
        bfd_init();
        s_bfdInited = true;
    }

    if (m_bfd) {
        return true;
    }

    m_bfd = bfd_fopen(m_fileName.c_str(), NULL, "rb", -1);
    if (!m_bfd) {
        std::cerr << "Could not open bfd file " << m_fileName << " - ";
        std::cerr << bfd_errmsg(bfd_get_error()) << std::endl;
        return false;
    }

    if (!bfd_check_format (m_bfd, bfd_object)) {
        std::cerr << m_fileName << " has invalid format " << std::endl;
        bfd_close(m_bfd);
        m_bfd = NULL;
        return false;
    }

    long storage_needed = bfd_get_symtab_upper_bound (m_bfd);
    long number_of_symbols;

    if (storage_needed < 0) {
        std::cerr << "Failed to determine needed storage" << std::endl;
        bfd_close(m_bfd);
        m_bfd = NULL;
        return false;
    }

    m_symbolTable = (asymbol**)malloc (storage_needed);
    number_of_symbols = bfd_canonicalize_symtab (m_bfd, m_symbolTable);
    if (number_of_symbols < 0) {
        std::cerr << "Failed to determine number of symbols" << std::endl;
        bfd_close(m_bfd);
        m_bfd = NULL;
        return false;
    }

    if (m_requireSymbols && !(m_bfd->flags & HAS_SYMS)) {
        return false;
    }

    bfd_map_over_sections(m_bfd, initSections, this);

    //Compute image base
    //XXX: Make sure it is correct
    Sections::const_iterator it;
    uint64_t vma=(uint64_t)-1;
    for (it = m_sections.begin(); it != m_sections.end(); ++it) {
        asection *section = (*it).second;
        if (section->vma && (section->vma < vma)) {
            vma = section->vma;
        }
    }
    assert(vma);
    m_imageBase = vma & (uint64_t)~0xFFF;

    //Extract module name
    size_t pos = m_fileName.find_last_of("\\/");
    if (pos == std::string::npos) {
        m_moduleName = m_fileName;
    }else {
        m_moduleName = m_fileName.substr(pos);
    }

    return true;
}

bool BFDInterface::getInfo(uint64_t addr, std::string &source, uint64_t &line, std::string &function)
{
    if (!initialize()) {
        return false;
    }

    BFDSection s;
    s.start = addr;
    s.size = 1;


    if (m_invalidAddresses.find(addr) != m_invalidAddresses.end()) {
        return false;
    }

    Sections::const_iterator it = m_sections.find(s);
    if (it == m_sections.end()) {
        std::cerr << "Could not find section at address 0x"  << std::hex << addr << " in file " << m_fileName << std::endl;
        //Cache the value for speed  up
        m_invalidAddresses.insert(addr);
        return false;
    }

    asection *section = (*it).second;
    //std::cout << "Section " << section->name << " " << std::hex << section->vma << " - size=0x"  << section->size <<
    //        " for address " << addr << std::endl;

    const char *filename;
    const char *funcname;
    unsigned int sourceline;

    if (bfd_find_nearest_line(m_bfd, section, m_symbolTable, addr - section->vma,
        &filename, &funcname, &sourceline)) {

        source = filename ? filename : "<unknown source>" ;
        line = sourceline;
        function = funcname ? funcname:"<unknown function>";

        if (!filename && !line && !funcname) {
            return false;
        }
        return true;

    }

    return false;

}

bool BFDInterface::getModuleName(std::string &name ) const
{
    if (!m_bfd) {
        return false;
    }

    name = m_moduleName;
    return true;
}

uint64_t BFDInterface::getImageBase() const
{
    if (!m_bfd) {
        return false;
    }

    return m_imageBase;
}

uint64_t BFDInterface::getImageSize() const
{
    if (!m_bfd) {
        return false;
    }

    return bfd_get_size(m_bfd);
}

uint64_t BFDInterface::getEntryPoint() const
{
    if (!m_bfd) {
        return 0;
    }

    return m_bfd->start_address;
}

bool BFDInterface::read(uint64_t va, void *dest, unsigned size)
{
    if (!m_bfd) {
        return false;
    }

    BFDSection s;
    s.start = va;
    s.size = 1;

    Sections::const_iterator it = m_sections.find(s);
    if (it == m_sections.end()) {
        return false;
    }

    asection *section = (*it).second;

    return bfd_get_section_contents(m_bfd, section, dest, va - section->vma, size);
}

}
