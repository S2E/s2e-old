#ifndef _WINDBG_BFDINTERFACE_

#define _WINDBG_BFDINTERFACE_

extern "C" {
#include <bfd.h>
}

#include <string>
#include <windows.h>

#undef __specstrings
#include <dbgeng.h>
#define __specstrings

#include <inttypes.h>
#include <map>
#include <set>


#include "StartSize.h"

#define DBGPRINT(...) Control->Output(DEBUG_OUTPUT_NORMAL, __VA_ARGS__);

class BFDInterface {
private:
    typedef std::map<StartSize, asection*> Sections;
    typedef std::map<StartSize, unsigned> AddressRangeToSymbolIndex;
    typedef std::map<uint64_t, unsigned> AddressToSymbolIndex;

    bfd *m_bfd;
    bfd_symbol **m_symbolTable;
    unsigned m_symbolCount;
    Sections m_sections;
    AddressRangeToSymbolIndex m_mappedSymbols;
    std::set<uint64_t> m_invalidAddresses;
    uint64_t m_imageBase;

    const char *Strip(const char *c) const;

    bool MapSymbols();

    static void InitSections(bfd *abfd, asection *sect, void *obj);
    bool InitImageBase(const char *fn);
public:
    BFDInterface(IDebugControl *Control, const char *ImageName);
    bool UpdateSymbols(IDebugControl *Control, IDebugSymbols3 *Symbols);
    bool IsInitialized() const {
        return m_bfd != NULL;
    }
    uint64_t GetImageSize() const;
    bool GetInfo(uint64_t addr, std::string &source, uint64_t &line, std::string &function);
    bool GetSymbolForAddress(uint64_t addr, StartSize &sz, std::string &s) const;

    uint64_t GetImageBase() const {
        return m_imageBase;
    }

    void DumpSymbols(IDebugControl *Control) const;
};


#endif
