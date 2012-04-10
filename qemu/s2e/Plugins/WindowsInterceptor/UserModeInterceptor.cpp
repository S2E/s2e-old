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
#include "qemu-common.h"
}

#include "UserModeInterceptor.h"
#include "WindowsImage.h"

#include <s2e/Utils.h>
#include <s2e/S2E.h>

#include <string>

using namespace s2e;
using namespace plugins;

WindowsUmInterceptor::WindowsUmInterceptor(WindowsMonitor *Os)
{

    m_Os = Os;
    m_TracingState = SEARCH_PROCESS;
    m_PrevCr3 = 0;

    m_ASBase = 0;
    m_ASSize = Os->GetUserAddressSpaceSize();
}


WindowsUmInterceptor::~WindowsUmInterceptor()
{

}

#if 0
/**
*  Cycle through the list of all loaded processes and notify the listeners
*/
bool WindowsUmInterceptor::NotifyLoadedProcesses(S2EExecutionState *state)
{
    s2e::windows::LIST_ENTRY32 ListHead;
    uint64_t ActiveProcessList = m_Os->GetPsActiveProcessListPtr();
    CPUState *cpuState = (CPUState *)state->getCpuState();

    uint64_t pListHead = PsLoadedModuleList;
    if (!state->readMemoryConcrete(ActiveProcessList, &ListHead, sizeof(ListHead))) {
        return false;
    }

    for (pItem = ListHead.Flink; pItem != pListHead; ) {
        uint32_t pProcessEntry = CONTAINING_RECORD32(pItem, s2e::windows::EPROCESS32, ActiveProcessLinks);
        s2e::windows::EPROCESS32 ProcessEntry;

        if (!state->readMemoryConcrete(pProcessEntry, &ProcessEntry, sizeof(ProcessEntry))) {
            return false;
        }

        ModuleDescriptor desc;
        state->readString(ProcessEntry.ImageFileName, desc.Name, sizeof(ProcessEntry.ImageFileName));
        desc.Pid = ProcessEntry.Pcb.DirectoryTableBase;
        desc.LoadBase = ProcessEntry.Pcb. LdrEntry.DllBase;
        desc.Size = LdrEntry.SizeOfImage;


    }
}
#endif

bool WindowsUmInterceptor::FindModules(S2EExecutionState *state)
{
    s2e::windows::LDR_DATA_TABLE_ENTRY32 LdrEntry;
    s2e::windows::PEB_LDR_DATA32 LdrData;

    if (!WaitForProcessInit(state)) {
        return false;
    }

    if (!state->readMemoryConcrete(m_LdrAddr, &LdrData, sizeof(s2e::windows::PEB_LDR_DATA32))) {
        return false;
    }

    uint32_t CurLib = CONTAINING_RECORD32(LdrData.InLoadOrderModuleList.Flink,
        s2e::windows::LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);

    uint32_t HeadOffset = m_LdrAddr + offsetof(s2e::windows::PEB_LDR_DATA32, InLoadOrderModuleList);
    if (LdrData.InLoadOrderModuleList.Flink == HeadOffset) {
        return false;
    }

    do {
        if (!state->readMemoryConcrete(CurLib, &LdrEntry, sizeof(s2e::windows::LDR_DATA_TABLE_ENTRY32))) {
            s2e_debug_print("Could not read LDR_DATA_TABLE_ENTRY (%#x)\n", CurLib);
            return false;
        }

        std::string s;
        state->readUnicodeString(LdrEntry.BaseDllName.Buffer, s, LdrEntry.BaseDllName.Length);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);

        if (s.length() == 0) {
            if (LdrEntry.DllBase == 0x7c900000) {
                //XXX
                s = "ntdll.dll";
            }else {
                s = "<unnamed>";
            }
        }

        //if (m_SearchedModules.find(s) != m_SearchedModules.end()) {
        //Update the information about the library
        ModuleDescriptor Desc;
        Desc.Pid = state->getPid();
        Desc.Name = s;
        Desc.LoadBase = LdrEntry.DllBase;
        Desc.Size = LdrEntry.SizeOfImage;

        //XXX: this must be state-local
        if (m_LoadedLibraries.find(Desc) == m_LoadedLibraries.end()) {
            s2e_debug_print("  MODULE %s Base=%#x Size=%#x\n", s.c_str(), LdrEntry.DllBase, LdrEntry.SizeOfImage);
            m_LoadedLibraries.insert(Desc);
            NotifyModuleLoad(state, Desc);
        }

        CurLib = CONTAINING_RECORD32(LdrEntry.InLoadOrderLinks.Flink,
            s2e::windows::LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);
    }while(LdrEntry.InLoadOrderLinks.Flink != HeadOffset);

    return true;
}


bool WindowsUmInterceptor::WaitForProcessInit(S2EExecutionState* state)
{
    s2e::windows::PEB_LDR_DATA32 LdrData;
    s2e::windows::PEB32 PebBlock;
    uint32_t Peb = (uint32_t)-1;


    uint64_t fsBase = state->readCpuState(CPU_OFFSET(segs[R_FS].base), 8*sizeof(target_ulong));


    if (state->getPc() < 0x80000000) {
        if(!state->readMemoryConcrete(fsBase + 0x18, &Peb, 4)) {
            return false;
        }

        if(!state->readMemoryConcrete(Peb+0x30, &Peb, 4)) {
            return false;
        }
    }else {
        //We are in kernel mode, do it by reading kernel-mode struc
        uint32_t curProcess = -1;

        curProcess = m_Os->getCurrentProcess(state);
        if (!curProcess) {
            return false;
        }

        Peb = m_Os->getPeb(state, curProcess);
    }

    if (Peb == 0xFFFFFFFF) {
        return false;
    }

    if (!state->readMemoryConcrete(Peb, &PebBlock, sizeof(PebBlock))) {
        return false;
    }

    /* Check that the entries are inited */
    if (!state->readMemoryConcrete(PebBlock.Ldr, &LdrData,
        sizeof(s2e::windows::PEB_LDR_DATA32))) {
            return false;
    }

    /* Check that the structure is correctly initialized */
    if (m_Os->getBuildNumber() >= BUILD_LONGHORN) {
        if (LdrData.Length != 0x30)
            return false;
    }else {
        if (LdrData.Length != 0x28)
            return false;
    }

    if (!LdrData.InLoadOrderModuleList.Flink || !LdrData.InLoadOrderModuleList.Blink )
        return false;

    if (!LdrData.InMemoryOrderModuleList.Flink || !LdrData.InMemoryOrderModuleList.Blink )
        return false;

    m_LdrAddr = PebBlock.Ldr;
    m_ProcBase = PebBlock.ImageBaseAddress;

    s2e_debug_print("Process %#"PRIx64" %#x %#x\n", m_ProcBase, LdrData.Initialized, LdrData.EntryInProgress);
    return true;

}


void WindowsUmInterceptor::NotifyModuleLoad(S2EExecutionState *state, ModuleDescriptor &Library)
{
    WindowsImage Image(state, Library.LoadBase);
    Library.NativeBase = Image.GetImageBase();
    Library.EntryPoint = Image.GetEntryPoint() + Library.NativeBase;
    m_Os->onModuleLoad.emit(state, Library);
}

bool WindowsUmInterceptor::CatchModuleLoad(S2EExecutionState *State)
{
    FindModules(State);
    return true;
}

bool WindowsUmInterceptor::CatchProcessTerminationXp(S2EExecutionState *State)
{
    uint64_t pEProcess;

    assert(m_Os->GetVersion() == WindowsMonitor::XPSP3);

    pEProcess = cast<klee::ConstantExpr>(
        State->readCpuRegister(CPU_OFFSET(regs[R_EBX]), 8*sizeof(target_ulong)))
            ->getZExtValue();
    s2e::windows::EPROCESS32_XP EProcess;

    if (!State->readMemoryConcrete(pEProcess, &EProcess, sizeof(EProcess))) {
        TRACE("Could not read EProcess data structure at %#"PRIx64"!\n", pEProcess);
        return false;
    }

    s2e_debug_print("Process %#"PRIx32" %16s unloaded\n", EProcess.Pcb.DirectoryTableBase,
        EProcess.ImageFileName);
    m_Os->onProcessUnload.emit(State, EProcess.Pcb.DirectoryTableBase);

    return true;
}

bool WindowsUmInterceptor::CatchProcessTerminationServer2008(S2EExecutionState *State)
{
    uint64_t pThread, pProcess;

    assert(m_Os->GetVersion() == WindowsMonitor::SRV2008SP2);

    pThread = cast<klee::ConstantExpr>(
        State->readCpuRegister(CPU_OFFSET(regs[R_ESI]), 8*sizeof(target_ulong)))
            ->getZExtValue();


    if (!State->readMemoryConcrete(pThread + ETHREAD_PROCESS_OFFSET_VISTA, &pProcess, sizeof(pProcess))) {
        return false;
    }

    s2e::windows::EPROCESS32_XP EProcess;

    if (!State->readMemoryConcrete(pProcess, &EProcess, sizeof(EProcess))) {
        TRACE("Could not read EProcess data structure at %#"PRIx64"!\n", pProcess);
        return false;
    }

    s2e_debug_print("Process %#"PRIx32" %16s unloaded\n", EProcess.Pcb.DirectoryTableBase,
        EProcess.ImageFileName);
    m_Os->onProcessUnload.emit(State, EProcess.Pcb.DirectoryTableBase);

    return true;
}

bool WindowsUmInterceptor::CatchProcessTermination(S2EExecutionState *State)
{
    switch(m_Os->GetVersion()) {
        case WindowsMonitor::XPSP3: return CatchProcessTerminationXp(State);
        case WindowsMonitor::SRV2008SP2: return CatchProcessTerminationServer2008(State);
        default: assert(false && "Unsupported OS");
    }
    return false;
}



bool WindowsUmInterceptor::CatchModuleUnloadBase(S2EExecutionState *State, uint64_t pLdrEntry)
{
    s2e::windows::LDR_DATA_TABLE_ENTRY32 LdrEntry;

    if (!State->readMemoryConcrete(pLdrEntry, &LdrEntry, sizeof(LdrEntry))) {
        TRACE("Could not read pLdrEntry data structure at %#"PRIx64"!\n", pLdrEntry);
        return false;
    }


    ModuleDescriptor Desc;
    Desc.Pid = State->getPc();

    State->readUnicodeString(LdrEntry.BaseDllName.Buffer, Desc.Name, LdrEntry.BaseDllName.Length);;
    std::transform(Desc.Name.begin(), Desc.Name.end(), Desc.Name.begin(), ::tolower);

    Desc.LoadBase = LdrEntry.DllBase;
    Desc.Size = LdrEntry.SizeOfImage;

    s2e_debug_print("Detected module unload %s pid=%#"PRIx64" LoadBase=%#"PRIx64"\n",
        Desc.Name.c_str(), Desc.Pid, Desc.LoadBase);

    m_Os->onModuleUnload.emit(State, Desc);

    return true;
}

bool WindowsUmInterceptor::CatchModuleUnloadXPSP3(S2EExecutionState *State)
{
    assert(m_Os->GetVersion() == WindowsMonitor::XPSP3);
    uint64_t pLdrEntry = cast<klee::ConstantExpr>(
        State->readCpuRegister(CPU_OFFSET(regs[R_ESI]), 8*sizeof(target_ulong)))
            ->getZExtValue();

    return CatchModuleUnloadBase(State, pLdrEntry);
}

bool WindowsUmInterceptor::CatchModuleUnloadServer2008(S2EExecutionState *state)
{
    assert(m_Os->GetVersion() == WindowsMonitor::SRV2008SP2);

    uint64_t pLdrEntry = cast<klee::ConstantExpr>(
        state->readCpuRegister(CPU_OFFSET(regs[R_EAX]), 8*sizeof(target_ulong)))
            ->getZExtValue();

    //Check if the load count reached zero
    uint16_t loadCount;
    if (!state->readMemoryConcrete(pLdrEntry + 0x38, &loadCount, sizeof(loadCount))) {
        return false;
    }

    if (loadCount > 0) {
        //Module is still referenced
        return false;
    }

    return CatchModuleUnloadBase(state, pLdrEntry);
}

bool WindowsUmInterceptor::CatchModuleUnload(S2EExecutionState *State)
{
    switch(m_Os->GetVersion()) {
        case WindowsMonitor::XPSP3: return CatchModuleUnloadXPSP3(State);
        case WindowsMonitor::SRV2008SP2: return CatchModuleUnloadServer2008(State);
        default: assert(false && "Unsupported OS");
    }
    return false;
}

bool WindowsUmInterceptor::GetPids(S2EExecutionState *State, PidSet &out)
{
    windows::LIST_ENTRY32 ListHead;
    uint64_t ActiveProcessList = m_Os->GetPsActiveProcessListPtr();
    uint64_t pItem;

    uint64_t CurrentProcess = m_Os->getCurrentProcess(State);
    g_s2e->getDebugStream() << "CurrentProcess: " << hexval(CurrentProcess) << '\n';

    //Read the head of the list
    if (!State->readMemoryConcrete(ActiveProcessList, &ListHead, sizeof(ListHead))) {
        return false;
    }

    //Check for empty list
    if (ListHead.Flink == ActiveProcessList) {
        return true;
    }

    for (pItem = ListHead.Flink; pItem != ActiveProcessList; pItem = ListHead.Flink) {
        if (!State->readMemoryConcrete(pItem, &ListHead, sizeof(ListHead))) {
            return false;
        }

        uint32_t pProcessEntry = m_Os->getProcessFromLink(pItem);
        uint32_t pDirectoryTableBase = m_Os->getDirectoryTableBase(State, pProcessEntry);

//        g_s2e->getDebugStream() << "Found EPROCESS=0x" <<  pProcessEntry << " PgDir=0x" << std::hex << ProcessEntry.Pcb.DirectoryTableBase <<
//                ProcessEntry.ImageFileName << std::endl;

        out.insert(pDirectoryTableBase);
    }
    return true;
}


