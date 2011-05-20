#include "BFDInterface.h"

#include <inttypes.h>
using namespace std;


BFDInterface::BFDInterface(IDebugControl *Control, const char *ImageName)
{
    long storage_needed = 0;
    long number_of_symbols = 0;

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

    return;

    err2:
    free(m_symbolTable);
    err1:
    bfd_close(m_bfd);
    m_bfd = NULL;
}

const char *BFDInterface::Strip(const char *c)
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

        uint64_t address = m_symbolTable[i]->value + m_symbolTable[i]->section->vma;

        char *demangled = bfd_demangle(m_bfd, Strip(m_symbolTable[i]->name), 0);
        DBGPRINT("%s %p %x\n", m_symbolTable[i]->name, m_symbolTable[i]->value, m_symbolTable[i]->flags);
        DBGPRINT("%s %p\n", demangled, address);


        const char *sname = demangled ? demangled : m_symbolTable[i]->name;

        DEBUG_MODULE_AND_ID ModuleId;
        HRESULT hr = Symbols->AddSyntheticSymbol(address, 4, sname, DEBUG_ADDSYNTHSYM_DEFAULT, &ModuleId);
        if (FAILED(hr)) {
            DBGPRINT("Failed to add symbol (%p)\n", hr);
        }else {
            loadedCount++;
        }

        DBGPRINT("\n");

        if (demangled)
         free(demangled);

    }

    DBGPRINT("Loaded %d symbols\n", loadedCount);
    return true;
}


