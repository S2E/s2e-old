#ifndef _WINDBG_BFDINTERFACE_

#define _WINDBG_BFDINTERFACE_

extern "C" {
#include <bfd.h>
}

#include <windows.h>

#undef __specstrings
#include <dbgeng.h>
#define __specstrings

#define DBGPRINT(...) Control->Output(DEBUG_OUTPUT_NORMAL, __VA_ARGS__);

class BFDInterface {
private:
    bfd *m_bfd;
    bfd_symbol **m_symbolTable;
    unsigned m_symbolCount;

    const char *Strip(const char *c);
public:
    BFDInterface(IDebugControl *Control, const char *ImageName);
    bool UpdateSymbols(IDebugControl *Control, IDebugSymbols3 *Symbols);
    bool IsInitialized() const {
        return m_bfd != NULL;
    }
};


#endif
