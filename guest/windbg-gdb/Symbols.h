#ifndef _WINDBG_SYMBOLS_

#define _WINDBG_SYMBOLS_

#include <map>
#include <string>

#include "BFDInterface.h"
#include "StartSize.h"

class Symbols
{
private:
    typedef std::map<StartSize, BFDInterface*> ImagesByStart;
    typedef std::map<std::string, BFDInterface *> ImagesByName;

    ImagesByStart m_imagesByStart;
    ImagesByName m_imagesByName;

    BOOL LoadSymbolsImage(IDebugControl *Control,
                          IDebugSymbols3 *Symbols,
                          PSTR ImageName, ULONG ImageIndex);

    IDebugControl *Control;
public:
    Symbols(IDebugClient *client);
    ~Symbols();

    HRESULT LoadSymbols(IDebugClient *Client,
                        IDebugSymbols3 *Symbols);

    //Transforms the relative path to absolute
    static VOID FixPath(IDebugClient *Client,
                   IDebugControl *Control,
                   PSTR Path, ULONG PathLength);

    bool GetInfo(uint64_t addr,
                 std::string &source,
                 uint64_t &line,
                 std::string &function);

    bool GetSymbolForAddress(uint64_t addr, StartSize &sz, std::string &s) const;
    void Dump() const;

};

#endif
