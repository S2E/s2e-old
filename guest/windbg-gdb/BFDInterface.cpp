#include <inttypes.h>
#include <iostream>
#include <stdio.h>

#include "BFDInterface.h"


using namespace std;


BFDInterface::BFDInterface(IDebugControl *Control, const char *ImageName)
{
    long storage_needed = 0;
    long number_of_symbols = 0;
    uint64_t vma=(uint64_t)-1;
    Sections::const_iterator it;

    m_bfd = bfd_fopen(ImageName, NULL, "rb", -1);
    if (!m_bfd) {
        DBGPRINT("Could not open bfd file %s (%s)\n", ImageName, bfd_errmsg(bfd_get_error()));
        return;
    }

    if (!bfd_check_format (m_bfd, bfd_object)) {
        DBGPRINT("%s has invalid format\n", ImageName);
        goto err1;
    }

    storage_needed = bfd_get_symtab_upper_bound (m_bfd);

    if (storage_needed < 0) {
        DBGPRINT("Failed to determine needed storage\n");
        goto err1;
    }

    m_symbolTable = (asymbol**)malloc (storage_needed);
    if (!m_symbolTable) {
        DBGPRINT("Failed to allocate symbol storage\n");
        goto err1;
    }

    number_of_symbols = bfd_canonicalize_symtab (m_bfd, m_symbolTable);
    if (number_of_symbols < 0) {
        DBGPRINT("Failed to determine number of symbols\n");
        goto err2;
    }

    m_symbolCount = number_of_symbols;

    bfd_map_over_sections(m_bfd, InitSections, this);

    //Compute image base
    //XXX: Make sure it is correct
#if 0
    for (it = m_sections.begin(); it != m_sections.end(); ++it) {
        asection *section = (*it).second;
        if (section->vma && (section->vma < vma)) {
            vma = section->vma;
        }
    }
    m_imageBase = vma & (uint64_t)~0xFFF;
#endif
    if (!InitImageBase(ImageName)) {
        DBGPRINT("Could not compute image base\n");
    }else {
        DBGPRINT("Image base: %p\n", m_imageBase);
    }


    MapSymbols();

    return;

    err2:
    free(m_symbolTable);
    err1:
    bfd_close(m_bfd);
    m_bfd = NULL;
}

//XXX: only for 64-bit clients!
bool BFDInterface::InitImageBase(const char *fn)
{
    FILE *fp = fopen(fn, "rb");
    if (!fp) {
        return false;
    }

    IMAGE_DOS_HEADER DosHeader;
    if (fread(&DosHeader, sizeof(DosHeader), 1, fp) != 1) {
        fclose(fp);
        return false;
    }
    fseek(fp, DosHeader.e_lfanew, SEEK_SET);

    IMAGE_NT_HEADERS NtHeaders;
    if (fread(&NtHeaders, sizeof(NtHeaders), 1, fp) != 1) {
        fclose(fp);
        return false;
    }

    m_imageBase = NtHeaders.OptionalHeader.ImageBase;

    fclose(fp);
    return true;
}

void BFDInterface::InitSections(bfd *abfd, asection *sect, void *obj)
{
    BFDInterface *bfdptr = (BFDInterface*)obj;

    StartSize s;
    s.Start = sect->vma;
    s.Size = sect->size;

    //Deal with relocations
    if (bfd_get_section_flags(abfd, sect) & SEC_RELOC) {
        long reloc_size = bfd_get_reloc_upper_bound(bfdptr->m_bfd, sect);
        if (reloc_size > 0) {
            arelent **relent = (arelent**)malloc (reloc_size);
            long res = bfd_canonicalize_reloc(abfd, sect, relent, bfdptr->m_symbolTable);
            if (res < 0) {
                free(relent);
            }
        }
    }

    bfdptr->m_sections[s] = sect;
}

const char *BFDInterface::Strip(const char *c) const
{
    if (c[0] == '_' && c[1] == '_') {
        return c+1;
    }

    const char *old = c;
    while (*c && *c != '$') {
        ++c;
    }
    if (*c) {
        return c+1;
    }
    return old;
}

bool BFDInterface::MapSymbols()
{
    if (!IsInitialized()) {
        return false;
    }

    AddressToSymbolIndex syms;

    for (unsigned i=0; i<m_symbolCount; ++i) {
        if (!(m_symbolTable[i]->flags & BSF_FUNCTION)) {
            continue;
        }

        if (!(m_symbolTable[i]->section->flags & SEC_CODE)) {
            continue;
        }

        if (m_symbolTable[i]->section->symbol == m_symbolTable[i]) {
            continue;
        }

        uint64_t address = m_symbolTable[i]->value + m_symbolTable[i]->section->vma;

        syms[address] = i;

    }

    //Now initialize address ranges
    //Assume addresses are sorted by increasing order
    AddressToSymbolIndex::iterator it, it_next;
    for (it = syms.begin(); it != syms.end(); ++it) {
        it_next = it;
        ++it_next;

        uint64_t address = (*it).first;
        if (it_next != syms.end()) {
            uint64_t next_address = (*it_next).first;
            StartSize range(address, next_address - address);
            m_mappedSymbols[range] = (*it).second;
        }else {
            StartSize range(address, address + 1);
            m_mappedSymbols[range] = (*it).second;
        }
    }

    return true;
}

bool BFDInterface::GetSymbolForAddress(uint64_t addr, StartSize &sz, std::string &s) const
{
    StartSize range(addr, addr+1);
    AddressRangeToSymbolIndex::const_iterator it = m_mappedSymbols.find(range);
    if (it == m_mappedSymbols.end()) {
        return false;
    }

    sz = (*it).first;

    unsigned index = (*it).second;

    char *demangled = bfd_demangle(m_bfd, Strip(m_symbolTable[index]->name), 0);
    const char *sname = demangled ? demangled : m_symbolTable[index]->name;
    s = std::string(sname);

    if (demangled)
        free(demangled);
    return true;
}

bool BFDInterface::UpdateSymbols(IDebugControl *Control, IDebugSymbols3 *Symbols)
{
    if (!IsInitialized()) {
        return false;
    }

    unsigned loadedCount = 0;

    for (unsigned i=0; i<m_symbolCount; ++i) {
        if (!(m_symbolTable[i]->flags & BSF_FUNCTION)) {
            continue;
        }

        if (!(m_symbolTable[i]->section->flags & SEC_CODE)) {
            continue;
        }

        if (m_symbolTable[i]->section->symbol == m_symbolTable[i]) {
            continue;
        }

        if ((loadedCount % 20) == 0) {
            DBGPRINT("\rLoaded %d symbols...", loadedCount);
        }

        uint64_t address = m_symbolTable[i]->value + m_symbolTable[i]->section->vma;

        char *demangled = bfd_demangle(m_bfd, Strip(m_symbolTable[i]->name), 0);

        const char *sname = demangled ? demangled : m_symbolTable[i]->name;

        if (strstr(sname, "llvm::") || strstr(sname, "sigc::") || strstr(sname, "klee::")
            || strstr(sname, "anonymous") || strstr(sname, "std")) {
            continue;
        }

        //DBGPRINT("%s %p %x\n", sname, address, m_symbolTable[i]->flags);

        DEBUG_MODULE_AND_ID ModuleId;
        HRESULT hr = Symbols->AddSyntheticSymbol(address, 4, sname, DEBUG_ADDSYNTHSYM_DEFAULT, &ModuleId);
        if (FAILED(hr)) {
            //DBGPRINT("Failed to add symbol (%p)\n", hr);
        }else {
            loadedCount++;
        }

        loadedCount++;
        //DBGPRINT("\n");

        if (demangled)
         free(demangled);

    }

    DBGPRINT("Loaded %d symbols from the symbol table\n", loadedCount);
    return true;
}

uint64_t BFDInterface::GetImageSize() const
{
    if (!m_bfd) {
        return 0;
    }
    return bfd_get_size(m_bfd);
}

bool BFDInterface::GetInfo(uint64_t addr, std::string &source, uint64_t &line, std::string &function)
{
    if (!m_bfd) {
        return false;
    }

    StartSize s;
    s.Start = addr;
    s.Size = 1;


    if (m_invalidAddresses.find(addr) != m_invalidAddresses.end()) {
        return false;
    }

    Sections::const_iterator it = m_sections.find(s);
    if (it == m_sections.end()) {
        std::cerr << "Could not find section at address 0x"  << std::hex << addr  << std::endl;
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

void BFDInterface::DumpSymbols(IDebugControl *Control) const
{
    AddressRangeToSymbolIndex::const_iterator it;
    for (it = m_mappedSymbols.begin(); it != m_mappedSymbols.end(); ++it) {
        uint64_t Address = (*it).first.Start;
        std::string Symbol;
        StartSize sz;
        GetSymbolForAddress(Address, sz, Symbol);
        DBGPRINT("%p %s\n", Address + m_imageBase, Symbol.c_str());
    }
    DBGPRINT("\n");
}

