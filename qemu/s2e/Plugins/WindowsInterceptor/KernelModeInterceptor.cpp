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
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */


// XXX: qemu stuff should be included before anything from KLEE or LLVM !
extern "C" {
#include "config.h"
//#include "cpu.h"
//#include "exec-all.h"
#include "qemu-common.h"
}

#include "KernelModeInterceptor.h"
#include "WindowsImage.h"

#include <s2e/Utils.h>

#include <string>
#include <algorithm>

using namespace s2e;
using namespace plugins;

WindowsKmInterceptor::WindowsKmInterceptor(WindowsMonitor *Os)
{
  m_Os = Os;
}


WindowsKmInterceptor::~WindowsKmInterceptor()
{

}


void WindowsKmInterceptor::NotifyDriverLoad(S2EExecutionState *State, ModuleDescriptor &Desc)
{
    WindowsImage Image(State, Desc.LoadBase);

    Desc.Pid = 0;
    Desc.NativeBase = Image.GetImageBase();
    assert(Desc.NativeBase < 0x100000000);
    Desc.Size = Image.GetImageSize();
    Desc.EntryPoint = Image.GetEntryPoint() + Desc.NativeBase;
    Desc.Sections = Image.GetSections(State);

    if (!Desc.Size) {
        Desc.Size = m_Os->getModuleSizeFromCfg(Desc.Name);
    }

    m_Os->onModuleLoad.emit(State, Desc);
}

void WindowsKmInterceptor::NotifyDriverUnload(S2EExecutionState *State, const ModuleDescriptor &Desc)
{
    m_Os->onModuleUnload.emit(State, Desc);
}

bool WindowsKmInterceptor::ReadModuleList(S2EExecutionState *state)
{
    uint32_t pListHead, pItem, pModuleEntry;
    uint32_t PsLoadedModuleList;
    s2e::windows::LIST_ENTRY32 ListHead;
    s2e::windows::MODULE_ENTRY32 ModuleEntry;

    const windows::DBGKD_GET_VERSION64 &VersionBlock = m_Os->getVersionBlock();
    PsLoadedModuleList = VersionBlock.PsLoadedModuleList;

    pListHead = PsLoadedModuleList;
    if (!state->readMemoryConcrete(PsLoadedModuleList, &ListHead, sizeof(ListHead))) {
        return false;
    }


    for (pItem = ListHead.Flink; pItem != pListHead; ) {
        pModuleEntry = pItem;

        if (state->readMemoryConcrete(pModuleEntry, &ModuleEntry, sizeof(ModuleEntry)) < 0) {
            std::cout << "Could not load MODULE_ENTRY" << std::endl;
            return false;
        }

        ModuleDescriptor desc;

        desc.Pid = 0;

        state->readUnicodeString(ModuleEntry.driver_Name.Buffer, desc.Name, ModuleEntry.driver_Name.Length);
        std::transform(desc.Name.begin(), desc.Name.end(), desc.Name.begin(), ::tolower);

        //s2e_debug_print("DRIVER_OBJECT Start=%#x Size=%#x DriverName=%s\n", ModuleEntry.base,
        //    0, desc.Name.c_str());


        desc.NativeBase = 0; // Image.GetImageBase();
        desc.LoadBase = ModuleEntry.base;


        NotifyDriverLoad(state, desc);

        pItem = ListHead.Flink;
        if (!state->readMemoryConcrete(ListHead.Flink, &ListHead, sizeof(ListHead))) {
            return false;
        }
    }

    return true;

}

bool WindowsKmInterceptor::GetDriverDescriptor(S2EExecutionState *state,
                                               uint64_t pDriverObject, ModuleDescriptor &Desc)
{
    s2e::windows::DRIVER_OBJECT32 DrvObject;
    s2e::windows::MODULE_ENTRY32 Me;
    std::string ModuleName;

    if (!state->readMemoryConcrete(pDriverObject,
        &DrvObject, sizeof(DrvObject))) {
            s2e_debug_print("Could not load DRIVER_OBJECT\n");
            return false;
    }

    s2e_debug_print("DRIVER_OBJECT Start=%#x Size=%#x\n", DrvObject.DriverStart,
        DrvObject.DriverSize);

    if (DrvObject.DriverStart & 0xFFF) {
        s2e_debug_print("Warning: The driver is not loaded on a page boundary\n");
    }


    //Fetch MODULE_ENTRY
    if (!DrvObject.DriverSection) {
        s2e_debug_print("Null driver section");
        return false;
    }

    if (state->readMemoryConcrete(DrvObject.DriverSection, &Me, sizeof(Me)) < 0) {
        s2e_debug_print("Could not load MODULE_ENTRY\n");
        return false;
    }

    state->readUnicodeString(Me.driver_Name.Buffer, ModuleName, Me.driver_Name.Length);
    std::transform(ModuleName.begin(), ModuleName.end(), ModuleName.begin(), ::tolower);

    s2e_debug_print("DRIVER_OBJECT Start=%#x Size=%#x DriverName=%s\n", Me.base,
        0, ModuleName.c_str());

    Desc.Pid = 0;
    Desc.Name = ModuleName;
    Desc.NativeBase = 0; // Image.GetImageBase();
    Desc.LoadBase = DrvObject.DriverStart;
    Desc.Size = DrvObject.DriverSize;

    return true;
}

bool WindowsKmInterceptor::CatchModuleLoad(S2EExecutionState *state)
{
    assert(m_Os->GetVersion() == WindowsMonitor::XPSP3);

    uint64_t pDriverObject=0;

    if (!state->readMemoryConcrete(state->getSp(), &pDriverObject, m_Os->GetPointerSize())) {
        return false;
    }

    if (!pDriverObject) {
        s2e_debug_print("DriverObject is NULL\n");
        return false;
    }

    ModuleDescriptor desc;
    if (!GetDriverDescriptor(state, pDriverObject, desc)) {
        return false;
    }

    NotifyDriverLoad(state, desc);
    return true;
}

bool WindowsKmInterceptor::CatchModuleUnload(S2EExecutionState *state)
{
    uint64_t pDriverObject;

    if (!state->readMemoryConcrete(state->getSp() + 4, &pDriverObject, m_Os->GetPointerSize())) {
        return false;
    }

    if (!pDriverObject) {
        s2e_debug_print("DriverObject is NULL\n");
        return false;
    }

    ModuleDescriptor desc;
    if (!GetDriverDescriptor(state, pDriverObject, desc)) {
        return false;
    }

    NotifyDriverUnload(state, desc);

    return true;
}
