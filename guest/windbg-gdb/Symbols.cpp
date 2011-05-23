#include <windows.h>
#include <psapi.h>
#include "Symbols.h"

Symbols::Symbols(IDebugClient *client)
{
    Control = NULL;
    HRESULT Ret = client->QueryInterface(IID_IDebugControl, (LPVOID*)&Control);
    if(FAILED(Ret)) {
        return;
    }
}

Symbols::~Symbols()
{
    ImagesByName::iterator it;
    for (it = m_imagesByName.begin(); it != m_imagesByName.end(); ++it) {
        delete (*it).second;
    }
    Control->Release();
}

HRESULT Symbols::LoadSymbols(IDebugClient *Client, IDebugSymbols3 *Symbols)
{
    HRESULT Ret;
    ULONG LoadedModulesCount, UnloadedModulesCount;

    Ret = Symbols->GetNumberModules(&LoadedModulesCount, &UnloadedModulesCount);
    if (FAILED(Ret)) {
        DBGPRINT("Could not get number of modules\n");
        return Ret;
    }

    for (ULONG i=0; i<LoadedModulesCount; ++i) {
        CHAR ImageName[4096];
        CHAR ModuleName[4096];
        CHAR LoadedImageName[4096];

        Ret = Symbols->GetModuleNames(i, 0,
                                ImageName, sizeof(ImageName), 0,
                                ModuleName, sizeof(ModuleName), 0,
                                LoadedImageName, sizeof(LoadedImageName), 0);
        switch(Ret) {
            case S_FALSE:
                DBGPRINT("Some image names truncated for module %d\n", i);
                DBGPRINT("ImageName: %s\n", ImageName);
                continue;
            case S_OK:
                break;
            default:
                DBGPRINT("Error while getting info for module %d\n", i);
                continue;
        }

        DBGPRINT("Loading symbols for %s - %s - %s\n", ImageName, ModuleName, LoadedImageName);

        //Sometimes Windbg returns the full path in LoadedImageName instead of ImageName.
        //We take the longest string.
        /*if (strlen(ImageName) < strlen(LoadedImageName)) {
            LoadSymbolsImage(Control, Symbols, LoadedImageName);
        }else*/
        FixPath(Client, Control, ImageName, sizeof(ImageName));
        if (!strstr(ImageName, "\\") && strlen(ImageName) < strlen(LoadedImageName)) {
            LoadSymbolsImage(Control, Symbols, LoadedImageName, i);
        }else {
            LoadSymbolsImage(Control, Symbols, ImageName, i);
        }
    }

    return S_OK;
}

BOOL Symbols::LoadSymbolsImage(IDebugControl *Control,
                               IDebugSymbols3 *Symbols,
                               PSTR ImageName, ULONG ImageIndex)
{
    BFDInterface *bfd = NULL;
    ImagesByName::iterator it = m_imagesByName.find(std::string(ImageName));
    if (it != m_imagesByName.end()) {
        bfd = (*it).second;
    }else {
        bfd = new BFDInterface(Control, (const char*)ImageName);
        if (!bfd->IsInitialized()) {
            return FALSE;
        }
        m_imagesByName[ImageName] = bfd;

        StartSize info;
        if (FAILED(Symbols->GetModuleByIndex(ImageIndex, &info.Start))) {
            return FALSE;
        }
        info.Size = bfd->GetImageSize();
        m_imagesByStart[info] = bfd;
        DBGPRINT("Loaded %s (Start=%p Size=%p)\n", ImageName, info.Start, info.Size);
    }

    return TRUE;
    //XXX: Don't update symbols in the environment, too slow
    //return bfd->UpdateSymbols(Control, Symbols);
}


VOID Symbols::FixPath(IDebugClient *Client, IDebugControl *Control, PSTR Path, ULONG PathLength)
{
    if (strstr(Path, "\\")) {
        //Path looks absolute, OK
        return;
    }

    //Convert the path to absolute.
    //We look for all processes that have the current name and pick the path for it.
    //XXX: what if more processes have the same name?
    ULONG Id;
    HRESULT hr = Client->GetRunningProcessSystemIdByExecutableName(0, Path, DEBUG_GET_PROC_ONLY_MATCH, &Id);
    if (FAILED(hr)) {
        return;
    }

    //Retrieve the process details
    HANDLE Proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, Id);
    if (Proc == NULL) {
        DBGPRINT("Could not open process %d\n", Id);
        return;
    }

    if (!GetModuleFileNameEx(Proc, NULL, Path, PathLength)) {
        DBGPRINT("Could not get file name of process %d %p\n", Id, GetLastError());
        return;
    }

    CloseHandle(Proc);

    DBGPRINT("Found path %s\n", Path);
}

bool Symbols::GetInfo(uint64_t addr,
             std::string &source,
             uint64_t &line,
             std::string &function) {
    StartSize info;
    info.Start = addr;
    info.Size = 1;

    ImagesByStart::iterator it = m_imagesByStart.find(info);
    if (it == m_imagesByStart.end()) {
        //DBGPRINT("Could not find image at %p\n", addr);
        return false;
    }

    BFDInterface *bfd = (*it).second;
    return bfd->GetInfo(addr - (*it).first.Start + bfd->GetImageBase(), source, line, function);
}

bool Symbols::GetSymbolForAddress(uint64_t addr, StartSize &sz, std::string &s) const
{
    StartSize info;
    info.Start = addr;
    info.Size = 1;

    ImagesByStart::const_iterator it = m_imagesByStart.find(info);
    if (it == m_imagesByStart.end()) {
        //DBGPRINT("Could not find image at %p\n", addr);
        return false;
    }

    BFDInterface *bfd = (*it).second;
    uint64_t Address = addr - (*it).first.Start + bfd->GetImageBase();
    return bfd->GetSymbolForAddress(Address, sz, s);
}

void Symbols::Dump() const
{
    ImagesByName::const_iterator it;
    for(it = m_imagesByName.begin(); it != m_imagesByName.end(); ++it) {
        DBGPRINT("Dumping symbols for %s\n", (*it).first.c_str());
        BFDInterface *bfd = (*it).second;
        bfd->DumpSymbols(Control);
    }
}
