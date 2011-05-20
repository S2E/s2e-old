/************************************************************
 *
 *                   Debug Extensions
 *
 * Toby Opferman
 *
 ************************************************************/
#define INITGUID

#include <string>
#include <map>
#include <stdio.h>
#include <windows.h>

#include "BFDInterface.h"

using namespace std;



extern "C" HRESULT CALLBACK DebugExtensionInitialize(PULONG Version, PULONG Flags)
{
    *Version = 0x00010000;
    *Flags = 0;

    bfd_init();

    return S_OK;
}

//called when the target is connected or disconnected
extern "C" VOID CALLBACK DebugExtensionNotify(ULONG Notify, ULONG64 Argument)
{

}

typedef std::map<std::string, BFDInterface *> LoadedImages;
static LoadedImages s_loadedImages;

//cannot assume that a debug session is active at this point
extern "C" VOID CALLBACK DebugExtensionUninitialize(VOID)
{
    LoadedImages::iterator it;
    for (it = s_loadedImages.begin(); it != s_loadedImages.end(); ++it) {
        delete (*it).second;
    }
    s_loadedImages.clear();
}



BOOL LoadSymbolsImage(IDebugControl *Control, IDebugSymbols3 *Symbols, PSTR ImageName)
{
    BFDInterface *bfd = NULL;
    LoadedImages::iterator it = s_loadedImages.find(std::string(ImageName));
    if (it != s_loadedImages.end()) {
        bfd = (*it).second;
    }else {
        bfd = new BFDInterface(Control, (const char*)ImageName);
        if (!bfd->IsInitialized()) {
            return FALSE;
        }
        s_loadedImages[ImageName] = bfd;
    }

    return bfd->UpdateSymbols(Control, Symbols);
}

HRESULT LoadSymbols(IDebugClient *Client, IDebugControl *Control, IDebugSymbols3 *Symbols)
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
        if (strlen(ImageName) < strlen(LoadedImageName)) {
            LoadSymbolsImage(Control, Symbols, LoadedImageName);
        }else {
            LoadSymbolsImage(Control, Symbols, ImageName);
        }
    }

    return S_OK;
}

extern "C" HRESULT CALLBACK gdbsyms(PDEBUG_CLIENT Client, PCSTR args)
{
    IDebugControl *control;
    IDebugSymbols3 *symbols;
    HRESULT hr;

    hr = Client->QueryInterface(IID_IDebugControl, (LPVOID*)&control);
    if(FAILED(hr)) {
        goto err1;
    }

    //control->Output(DEBUG_OUTPUT_NORMAL, "test\n");
    hr = Client->QueryInterface(IID_IDebugSymbols3, (LPVOID*)&symbols);
    if(FAILED(hr)) {
        control->Output(DEBUG_OUTPUT_NORMAL, "Failed to obtain IDebugSymbols3 interface. Use a more recent debugger.");
        goto err2;
    }

    LoadSymbols(Client, control, symbols);

    symbols->Release();
    control->Release();
    return S_OK;

err2: control->Release();
err1: return DEBUG_EXTENSION_CONTINUE_SEARCH;
}

