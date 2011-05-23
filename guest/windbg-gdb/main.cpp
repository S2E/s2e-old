#define INITGUID

#include <string>
#include <map>
#include <stdio.h>
#include <windows.h>
#include <psapi.h>
#include <stdint.h>

#include "BFDInterface.h"
#include "Symbols.h"

using namespace std;

static Symbols *s_symbols = NULL;

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


//cannot assume that a debug session is active at this point
extern "C" VOID CALLBACK DebugExtensionUninitialize(VOID)
{
    if (s_symbols) {
        delete s_symbols;
    }
}



//Loads information about all images
extern "C" HRESULT CALLBACK gload(PDEBUG_CLIENT Client, PCSTR args)
{
    IDebugControl *control;
    IDebugSymbols3 *symbols;
    HRESULT hr;

    if (!s_symbols) {
        s_symbols = new Symbols(Client);
    }

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

    s_symbols->LoadSymbols(Client, symbols);

    symbols->Release();
    control->Release();
    return S_OK;

err2: control->Release();
err1: return DEBUG_EXTENSION_CONTINUE_SEARCH;
}

extern "C" HRESULT CALLBACK gdump(PDEBUG_CLIENT Client, PCSTR args)
{
    s_symbols->Dump();
    return S_OK;
}

/**
 *  Queries the specified address for the corresponding symbol
 *  and add the symbol to the symbol library.
 *  Syntax: gsym address
 */
extern "C" HRESULT CALLBACK gsym(PDEBUG_CLIENT Client, PCSTR args)
{
    HRESULT Ret;
    IDebugControl *Control;
    IDebugSymbols3 *Symbols;
    std::string File, Function, Symbol="<unksym>";
    uint64_t Line;
    StartSize sz;


    Ret = Client->QueryInterface(IID_IDebugControl, (LPVOID*)&Control);
    if(FAILED(Ret)) {
        goto err1;
    }

    Ret = Client->QueryInterface(IID_IDebugSymbols3, (LPVOID*)&Symbols);
    if(FAILED(Ret)) {
        DBGPRINT("Failed to obtain IDebugSymbols3 interface. Use a more recent debugger.");
        goto err2;
    }


    UINT64 Address;
    sscanf(args, "%llx", &Address);



    if (s_symbols->GetSymbolForAddress(Address, sz, Symbol)) {
        DEBUG_MODULE_AND_ID ModuleId;
        HRESULT Ret = Symbols->AddSyntheticSymbol(sz.Start, (ULONG)sz.Size, Symbol.c_str(), DEBUG_ADDSYNTHSYM_DEFAULT, &ModuleId);
        if (FAILED(Ret)) {
            DBGPRINT("Failed to add symbol (%p)\n", Ret);
        }
    }
    DBGPRINT("%p Size=%#x %s ", sz.Start, sz.Size, Symbol.c_str());

    if (s_symbols->GetInfo(Address, File, Line, Function)) {
        DBGPRINT("%s:%d (%s)",  File.c_str(), Line, Function.c_str());
    }
    DBGPRINT("\n");


    Symbols->Release();
    Control->Release();
    return S_OK;

    err2: Control->Release();
    err1: return DEBUG_EXTENSION_CONTINUE_SEARCH;
}

/**
 *  Displays the backtrace starting from current program counter
 */
extern "C" HRESULT CALLBACK gbt(PDEBUG_CLIENT Client, PCSTR args)
{
    IDebugRegisters *Registers;
    IDebugControl *Control;
    IDebugDataSpaces *Data;
    HRESULT Ret;

    if (!s_symbols) {
        s_symbols = new Symbols(Client);
    }

    Ret = Client->QueryInterface(IID_IDebugControl, (LPVOID*)&Control);
    if(FAILED(Ret)) {
        goto err1;
    }

    Ret = Client->QueryInterface(IID_IDebugRegisters, (LPVOID*)&Registers);
    if (FAILED(Ret)) {
        goto err2;
    }

    Ret = Client->QueryInterface(IID_IDebugDataSpaces, (LPVOID*)&Data);
    if (FAILED(Ret)) {
        goto err3;
    }


    ULONG64 StackPointer;
    Registers->GetStackOffset(&StackPointer);

    DBGPRINT("Printing stack from offset %p\n", StackPointer)
    for(uint64_t s = StackPointer; s < StackPointer + 0x1000; s+=8) {
        std::string File, Function, Symbol="<unksym>";
        uint64_t Line;
        StartSize sz;

        uint64_t Address;
        ULONG ReadBytes;
        Data->ReadVirtual(s, &Address, sizeof(Address), &ReadBytes);

        s_symbols->GetSymbolForAddress(Address, sz, Symbol);

        DBGPRINT("[%p]=%p %s ", s, Address, Symbol.c_str());
        if (s_symbols->GetInfo(Address, File, Line, Function)) {
            DBGPRINT("%s:%d (%s)",  File.c_str(), Line, Function.c_str());
        }
        DBGPRINT("\n");
    }

    Data->Release();
    Control->Release();
    Registers->Release();

    return S_OK;

    err3: Registers->Release();
    err2: Control->Release();
    err1: return DEBUG_EXTENSION_CONTINUE_SEARCH;

}
