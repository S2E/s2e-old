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

#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>
#include "WindowsMonitor.h"
#include "WindowsImage.h"
#include "UserModeInterceptor.h"
#include "KernelModeInterceptor.h"

#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <assert.h>

using namespace std;

using namespace s2e;
using namespace s2e::plugins;

S2E_DEFINE_PLUGIN(WindowsMonitor, "Plugin for monitoring Windows kernel/user-mode events", "Interceptor");

//These are the keys to specify in the configuration file
const char *WindowsMonitor::s_windowsKeys[] =    {"XPSP2", "XPSP3",
                                         "XPSP2-CHK", "XPSP3-CHK", "SRV2008SP2"};

//These are user-friendly strings displayed to the user
const char *WindowsMonitor::s_windowsStrings[] =
{"Windows XP SP2 RTM",          "Windows XP SP3 RTM",
 "Windows XP SP2 Checked",      "Windows XP SP3 Checked",
 "Windows Server 2008 SP2 RTM"};

bool WindowsMonitor::s_checkedMap[] = {false, false, true, true};

unsigned WindowsMonitor::s_pointerSize[] =     {4,4,4,4,4};
uint64_t WindowsMonitor::s_kernelNativeBase[]= {0x00000000,  0x00400000,  0x00000000, 0x00400000, 0x00400000};
//uint64_t WindowsMonitor::s_kernelLoadBase[]=   {0x00000000,  0x804d7000,  0x00000000, 0x80a02000, 0x81836000};

uint64_t WindowsMonitor::s_ntdllNativeBase[]=  {0x7c900000,  0x00000000,  0x7c900000, 0x00000000, 0x77ed0000};
uint64_t WindowsMonitor::s_ntdllLoadBase[]=    {0x7c900000,  0x00000000,  0x7c900000, 0x00000000, 0x77ed0000};
uint64_t WindowsMonitor::s_ntdllSize[]=        {0x00000000,  0x00000000,  0x0007a000, 0x00000000, 0x001257F8};

uint64_t WindowsMonitor::s_driverLoadPc[] =    {0x00000000, 0x004cc99a, 0x00000000, 0x0053d5d6, 0x00563b82};
uint64_t WindowsMonitor::s_driverDeletePc[] =  {0x00000000, 0x004EB33F, 0x00000000, 0x00540a72, 0x0054217F};
uint64_t WindowsMonitor::s_kdDbgDataBlock[] =  {0x00000000, 0x00475DE0, 0x00000000, 0x004ec3f0, 0x004eec98};

uint64_t WindowsMonitor::s_panicPc1[] =        {0x00000000, 0x0045BCAA, 0x00000000, 0x0042f478, 0x004BBE83};
uint64_t WindowsMonitor::s_panicPc2[] =        {0x00000000, 0x0045C7CD, 0x00000000, 0x0042ff44, 0x004BB857};
uint64_t WindowsMonitor::s_panicPc3[] =        {0x00000000, 0x0045C7F3, 0x00000000, 0x0042ff62, 0x004BB87B};

uint64_t WindowsMonitor::s_ntTerminateProc[] = {0x00000000, 0x004ab3c8, 0x00000000, 0x005dab73, 0x0061913f};

uint64_t WindowsMonitor::s_sysServicePc[] =    {0x00000000, 0x00407631, 0x00000000, 0x004dca05, 0x0045777E};

uint64_t WindowsMonitor::s_psProcListPtr[] =   {0x00000000, 0x0048A358, 0x00000000, 0x005102b8, 0x00504150};

uint64_t WindowsMonitor::s_ldrpCall[] =        {0x7C901193, 0x7C901176, 0x00000000, 0x00000000, 0x77F11698};
uint64_t WindowsMonitor::s_dllUnloadPc[] =     {0x00000000, 0x7c91e12a, 0x00000000, 0x00000000, 0x77F0BB58};


WindowsMonitor::~WindowsMonitor()
{
    if (m_UserModeInterceptor) {
        delete m_UserModeInterceptor;
    }

    if (m_KernelModeInterceptor) {
        delete m_KernelModeInterceptor;
    }
}

void WindowsMonitor::initialize()
{
    string Version = s2e()->getConfig()->getString(getConfigKey() + ".version");
    m_UserMode = s2e()->getConfig()->getBool(getConfigKey() + ".userMode");
    m_KernelMode = s2e()->getConfig()->getBool(getConfigKey() + ".kernelMode");

    //For debug purposes
    m_MonitorModuleLoad = s2e()->getConfig()->getBool(getConfigKey() + ".monitorModuleLoad");
    m_MonitorModuleUnload = s2e()->getConfig()->getBool(getConfigKey() + ".monitorModuleUnload");
    m_MonitorProcessUnload = s2e()->getConfig()->getBool(getConfigKey() + ".monitorProcessUnload");

    m_KernelBase = GetKernelStart();
    m_FirstTime = true;
    //XXX: do it only when resuming a snapshot.
    m_TrackPidSet = true;

    unsigned i;
    for (i=0; i<(unsigned)MAXVER; ++i) {
        if (Version == s_windowsKeys[i]) {
            m_Version = (EWinVer)i;
            break;
        }
    }

    if (i == (EWinVer)MAXVER) {
        s2e()->getWarningsStream() << "Invalid windows version: " << Version << '\n';
        s2e()->getWarningsStream() << "Available versions are:" << '\n';
        for (unsigned j=0; j<MAXVER; ++j) {
            s2e()->getWarningsStream() << s_windowsKeys[j] << ":\t" << s_windowsStrings[j] << '\n';
        }
        exit(-1);
    }

    switch(m_Version) {
        case XPSP2_CHK:
        case XPSP3_CHK:
            s2e()->getWarningsStream() << "You specified a checked build of Windows XP." <<
                    "Only kernel-mode interceptors are supported for now." << '\n';
            break;
        default:
            break;
    }

    m_pKPCRAddr = 0;
    m_pKPRCBAddr = 0;

    m_UserModeInterceptor = NULL;
    m_KernelModeInterceptor = NULL;

    if (m_UserMode) {
        m_UserModeInterceptor = new WindowsUmInterceptor(this);
    }

    if (m_KernelMode) {
        m_KernelModeInterceptor = new WindowsKmInterceptor(this);
    }

    s2e()->getCorePlugin()->onTranslateInstructionStart.connect(
        sigc::mem_fun(*this, &WindowsMonitor::slotTranslateInstructionStart));

    s2e()->getCorePlugin()->onTranslateInstructionEnd.connect(
        sigc::mem_fun(*this, &WindowsMonitor::slotTranslateInstructionEnd));

    readModuleCfg();
}

void WindowsMonitor::readModuleCfg()
{
    std::vector<std::string> Sections;
    Sections = s2e()->getConfig()->getListKeys(getConfigKey() + ".modules");

    for (unsigned i=0; i<Sections.size(); i++) {
        std::stringstream sk;
        sk << getConfigKey() << ".modules." << Sections[i];

        std::string moduleName = s2e()->getConfig()->getString(sk.str() + ".name");
        uint64_t moduleSize = s2e()->getConfig()->getInt(sk.str() + ".size");
        m_ModuleInfo[moduleName] = moduleSize;
    }
}

void WindowsMonitor::InitializeAddresses(S2EExecutionState *state)
{
    if (m_pKPCRAddr) {
        return;
    }

    //Compute the address of the KPCR
    //It is located in fs:0x1c
    uint64_t base = state->readCpuState(CPU_OFFSET(segs[R_FS].base), 32);
    if (!state->readMemoryConcrete(base + KPCR_FS_OFFSET, &m_pKPCRAddr, sizeof(m_pKPCRAddr))) {
        s2e()->getWarningsStream() << "WindowsMonitor: Failed to initialize KPCR" << '\n';
        goto error;
    }

    //Read the version block
    uint32_t pKdVersionBlock;
    if (!state->readMemoryConcrete(m_pKPCRAddr + KPCR_KDVERSION32_OFFSET, &pKdVersionBlock, sizeof(pKdVersionBlock))) {
        s2e()->getWarningsStream() << "WindowsMonitor: Failed to read KD version block pointer" << '\n';
        goto error;
    }

    if (!state->readMemoryConcrete(pKdVersionBlock, &m_kdVersion, sizeof(m_kdVersion))) {
        s2e()->getWarningsStream() << "WindowsMonitor: Failed to read KD version block" << '\n';
        goto error;
    }

    //Read the KPRCB
    if (!state->readMemoryConcrete(m_pKPCRAddr + KPCR_KPRCB_PTR_OFFSET, &m_pKPRCBAddr, sizeof(m_pKPRCBAddr))) {
        s2e()->getWarningsStream() << "WindowsMonitor: Failed to read pointer to KPRCB" << '\n';
        goto error;
    }

    if (m_pKPRCBAddr != m_pKPCRAddr + KPCR_KPRCB_OFFSET) {
        s2e()->getWarningsStream () << "WindowsMonitor: Invalid KPRCB" << '\n';
        goto error;
    }

    if (!state->readMemoryConcrete(m_pKPRCBAddr, &m_kprcb, sizeof(m_kprcb))) {
        s2e()->getWarningsStream() << "WindowsMonitor: Failed to read KPRCB" << '\n';
        goto error;
    }

    //Display some info
    s2e()->getMessagesStream() << "Windows " << hexval(m_kdVersion.MinorVersion) <<
            (m_kdVersion.MajorVersion == 0xF ? " FREE BUILD" : " CHECKED BUILD") << '\n';

    return;

error:
    s2e()->getWarningsStream() << "Make sure you start S2E from a VM snapshot that has Windows already running." << '\n';
    exit(-1);

}

void WindowsMonitor::slotTranslateInstructionStart(ExecutionSignal *signal,
                                                   S2EExecutionState *state,
                                                   TranslationBlock *tb,
                                                   uint64_t pc)
{
    //XXX: on resume vm snapshot, the init routines may not be called.
    //However, when it is called, it will automatically scan all loaded modules.
    if(m_UserMode) {
        if (m_FirstTime) {
            InitializeAddresses(state);
            m_UserModeInterceptor->GetPids(state, m_PidSet);
            m_UserModeInterceptor->CatchModuleLoad(state);
            m_PidSet.erase(state->getPid());
        }

        if (pc == GetLdrpCallInitRoutine() && m_MonitorModuleLoad) {
            DPRINTF("Basic block for LdrpCallInitRoutine %#"PRIx64"\n", pc);
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotUmCatchModuleLoad));
        }else if (pc == GetNtTerminateProcessEProcessPoint() && m_MonitorProcessUnload) {
            DPRINTF("Basic block for NtTerminateProcess %#"PRIx64"\n", pc);
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotUmCatchProcessTermination));
        }else if (pc == GetDllUnloadPc() && m_MonitorModuleUnload) {
            DPRINTF("Basic block for Dll unload %#"PRIx64"\n", pc);
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotUmCatchModuleUnload));
        }
    }

    if(m_KernelMode) {
        //XXX: a module load can be notified twice if it was being loaded while the snapshot was saved.
        if (m_FirstTime) {
            slotKmUpdateModuleList(state, pc);
        }

        if (pc == GetDriverLoadPc()) {
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotKmModuleLoad));
        }else if (pc == GetDeleteDriverPc()) {
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotKmModuleUnload));
        }
    }

    m_FirstTime = false;
}

void WindowsMonitor::slotTranslateInstructionEnd(ExecutionSignal *signal,
    S2EExecutionState *state,
    TranslationBlock *tb,
    uint64_t pc)
{
    if (!m_TrackPidSet) {
        return;
    }

    if (!isTaskSwitch(state, pc)) {
        return;
    }

   signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotMonitorProcessSwitch));
}

void WindowsMonitor::slotMonitorProcessSwitch(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(WindowsMonitorState, state);

    if (m_PidSet.size() == 0) {
        m_TrackPidSet = false;
        return;
    }

    if (state->getPid() != plgState->m_CurrentPid) {
        plgState->m_CurrentPid = state->getPid();
        if (m_PidSet.find(state->getPid()) != m_PidSet.end()) {
            if (!m_UserModeInterceptor->CatchModuleLoad(state)) {
                //XXX: This is an ugly hack to force loading ntdll in all processes
                //ntdll.dll has a fixed addresse and used by all processes anyway.
                ModuleDescriptor ntdll;
                ntdll.Pid = state->getPid();
                ntdll.Name = "ntdll.dll";
                ntdll.NativeBase = s_ntdllNativeBase[m_Version]; // 0x7c900000;
                ntdll.LoadBase = s_ntdllLoadBase[m_Version];
                ntdll.Size = s_ntdllSize[m_Version];
                onModuleLoad.emit(state, ntdll);
            }
            m_PidSet.erase(state->getPid());
        }
    }
}

void WindowsMonitor::slotUmCatchModuleLoad(S2EExecutionState *state, uint64_t pc)
{
    DPRINTF("User mode module load at %#"PRIx64"\n", pc);
    m_UserModeInterceptor->CatchModuleLoad(state);
}

void WindowsMonitor::slotUmCatchModuleUnload(S2EExecutionState *state, uint64_t pc)
{
    DPRINTF("User mode module unload at %#"PRIx64"\n", pc);
    m_UserModeInterceptor->CatchModuleUnload(state);
}

void WindowsMonitor::slotUmCatchProcessTermination(S2EExecutionState *state, uint64_t pc)
{
    DPRINTF("Caught process termination\n");
    m_UserModeInterceptor->CatchProcessTermination(state);
}

void WindowsMonitor::slotKmModuleLoad(S2EExecutionState *state, uint64_t pc)
{
    DPRINTF("Kernel mode module load at %#"PRIx64"\n", pc);
    m_KernelModeInterceptor->CatchModuleLoad(state);
}

void WindowsMonitor::slotKmModuleUnload(S2EExecutionState *state, uint64_t pc)
{
    DPRINTF("Kernel mode module unload at %#"PRIx64"\n", pc);
    m_KernelModeInterceptor->CatchModuleUnload(state);
}

/**
 *  Scans the list of kernel modules and registers each entry.
 *  This is useful in case the VM snapshot was resumed (and all
 *  the modules are already loaded, but not registered with S2E).
 */
void WindowsMonitor::slotKmUpdateModuleList(S2EExecutionState *state, uint64_t pc)
{
    DPRINTF("Kernel mode module update at %#"PRIx64"\n", pc);
    if (m_KernelModeInterceptor->ReadModuleList(state)) {
        //List updated, unregister
        m_SyscallConnection.disconnect();
        m_FirstTime = false;
    }
}

bool WindowsMonitor::getImports(S2EExecutionState *s, const ModuleDescriptor &desc, Imports &I)
{
    if (desc.Pid && s->getPid() != desc.Pid) {
        return false;
    }

    WindowsImage Img(s, desc.LoadBase);
    I = Img.GetImports(s);
    return true;
}

bool WindowsMonitor::getExports(S2EExecutionState *s, const ModuleDescriptor &desc, Exports &E)
{
    if (desc.Pid && s->getPid() != desc.Pid) {
        return false;
    }

    WindowsImage Img(s, desc.LoadBase);
    E = Img.GetExports(s);
    return true;
}


//XXX: put in an appropriate place to share between different OSes.
//Detects whether the current instruction is a write to the Cr3 register
bool WindowsMonitor::isTaskSwitch(S2EExecutionState *state, uint64_t pc)
{
    uint64_t oldpc  = pc;
    uint8_t pref, reg;
    if (!state->readMemoryConcrete(pc++, &pref, 1)) {
        goto failure;
    }

    if (pref != 0x0F) {
        goto failure;
    }

    if (!state->readMemoryConcrete(pc++, &pref, 1)) {
        goto failure;
    }

    if (pref != 0x22) {
        goto failure;
    }

    if (!state->readMemoryConcrete(pc++, &pref, 1)) {
        goto failure;
    }

    reg = ((pref >> 3) & 7);
    if (reg != 3) {
        goto failure;
    }

    //We have got a task switch!
    s2e()->getDebugStream() << "Detected task switch at 0x" << oldpc << '\n';

    return true;

failure:
    //s2e()->getDebugStream() << "Could not read 0x" << std::hex << oldpc << " in isTaskSwitch" << std::dec << '\n';
    return false;
}

uint64_t WindowsMonitor::GetDriverLoadPc() const
{
    assert(s_driverLoadPc[m_Version]);
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_driverLoadPc[m_Version] + offset;
}

uint64_t WindowsMonitor::GetKdDebuggerDataBlock() const
{
    assert(s_kdDbgDataBlock[m_Version]);
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return (s_kdDbgDataBlock[m_Version] + offset);
}

bool WindowsMonitor::CheckPanic(uint64_t eip) const
{
    assert(s_panicPc1[m_Version] && s_panicPc2[m_Version] && s_panicPc3[m_Version]);
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return eip == s_panicPc1[m_Version] + offset ||
           eip == s_panicPc2[m_Version] + offset ||
           eip == s_panicPc3[m_Version] + offset;
}


uint64_t WindowsMonitor::GetUserAddressSpaceSize() const
{
    return 0x80000000;
}

uint64_t WindowsMonitor::GetKernelStart() const
{
    return 0x80000000;
}

uint64_t WindowsMonitor::GetLdrpCallInitRoutine() const
{
    assert(s_ldrpCall[m_Version] && "Unsupported OS version");
    uint64_t offset = s_ntdllLoadBase[m_Version] - s_ntdllNativeBase[m_Version];
    return s_ldrpCall[m_Version] + offset;
}

unsigned WindowsMonitor::GetPointerSize() const
{
    return s_pointerSize[m_Version];
}

uint64_t WindowsMonitor::GetKernelLoadBase() const
{
    if (GetPointerSize() == 4)
        return (uint32_t)m_kdVersion.KernBase;
    else
        return m_kdVersion.KernBase;
}

uint64_t WindowsMonitor::GetNtTerminateProcessEProcessPoint() const
{
    assert(s_ntTerminateProc[m_Version]);
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_ntTerminateProc[m_Version] + offset;
}

uint64_t WindowsMonitor::GetDeleteDriverPc() const
{
    assert(s_driverDeletePc[m_Version]);
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_driverDeletePc[m_Version] + offset;
}

uint64_t WindowsMonitor::GetSystemServicePc() const
{
    assert(s_sysServicePc[m_Version]);
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_sysServicePc[m_Version] + offset;
}

uint64_t WindowsMonitor::GetDllUnloadPc() const
{
    assert(s_dllUnloadPc[m_Version] && "Unsupported OS version");
    uint64_t offset = s_ntdllLoadBase[m_Version] - s_ntdllNativeBase[m_Version];
    return s_dllUnloadPc[m_Version] + offset;
}

uint64_t WindowsMonitor::GetPsActiveProcessListPtr() const
{
    assert(s_psProcListPtr[m_Version]);
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_psProcListPtr[m_Version] + offset;
}

bool WindowsMonitor::isKernelAddress(uint64_t pc) const
{
    //XXX: deal with large address space awareness
    return pc >= GetKernelStart();
}

uint64_t WindowsMonitor::getPid(S2EExecutionState *s, uint64_t pc)
{
    if (pc >= GetKernelStart()) {
        return 0;
    }
    return s->getPid();
}

uint64_t WindowsMonitor::getCurrentThread(S2EExecutionState *state)
{
    //It is located in fs:KPCR_CURRENT_THREAD_OFFSET
    uint64_t base = getTibAddress(state);
    uint32_t pThread = 0;
    if (!state->readMemoryConcrete(base + FS_CURRENT_THREAD_OFFSET, &pThread, sizeof(pThread))) {
        s2e()->getWarningsStream() << "Failed to get thread address" << '\n';
        return 0;
    }

    return pThread;
}

uint64_t WindowsMonitor::getCurrentProcess(S2EExecutionState *state)
{
    uint64_t pThread = getCurrentThread(state);
    if (!pThread) {
        return 0;
    }

    uint32_t threadOffset;
    if (m_kdVersion.MinorVersion >= BUILD_LONGHORN) {
        threadOffset = ETHREAD_PROCESS_OFFSET_VISTA;
    }else {
        threadOffset = ETHREAD_PROCESS_OFFSET_XP;
    }

    uint32_t pProcess = 0;
    if (!state->readMemoryConcrete(pThread + threadOffset, &pProcess, sizeof(pProcess))) {
        s2e()->getWarningsStream() << "Failed to get process address" << '\n';
        return 0;
    }

    return pProcess;
}

//Retrieves the current Thread Information Block, stored in the FS register
uint64_t WindowsMonitor::getTibAddress(S2EExecutionState *state)
{
    return state->readCpuState(CPU_OFFSET(segs[R_FS].base), 32);
}

bool WindowsMonitor::getTib(S2EExecutionState *state, s2e::windows::NT_TIB32 *tib)
{
    uint64_t tibAddress = getTibAddress(state);
    return state->readMemoryConcrete(tibAddress, &tib, sizeof(*tib));
}

uint64_t WindowsMonitor::getPeb(S2EExecutionState *state, uint64_t eprocess)
{
    uint32_t offset;
    if (m_kdVersion.MinorVersion >= BUILD_LONGHORN) {
        offset = offsetof(s2e::windows::EPROCESS32_VISTA,Peb);
    }else {
        offset = offsetof(s2e::windows::EPROCESS32_XP,Peb);
    }

    uint32_t peb = 0;
    if (!state->readMemoryConcrete(eprocess + offset, &peb, (sizeof(peb)))) {
        return 0;
    }
    return peb;
}

uint64_t WindowsMonitor::getProcessFromLink(uint64_t pItem)
{
    if (m_kdVersion.MinorVersion >= BUILD_LONGHORN) {
        return CONTAINING_RECORD32(pItem, s2e::windows::EPROCESS32_VISTA, ActiveProcessLinks);
    }else {
        return CONTAINING_RECORD32(pItem, s2e::windows::EPROCESS32_XP, ActiveProcessLinks);
    }
}

uint64_t WindowsMonitor::getDirectoryTableBase(S2EExecutionState *state, uint64_t pProcessEntry)
{
    if (m_kdVersion.MinorVersion >= BUILD_LONGHORN) {
        s2e::windows::EPROCESS32_VISTA ProcessEntry;

        if (!state->readMemoryConcrete(pProcessEntry, &ProcessEntry, sizeof(ProcessEntry))) {
            return 0;
        }

        return ProcessEntry.Pcb.DirectoryTableBase;
    }else {
        s2e::windows::EPROCESS32_XP ProcessEntry;

        if (!state->readMemoryConcrete(pProcessEntry, &ProcessEntry, sizeof(ProcessEntry))) {
            return 0;
        }

        return ProcessEntry.Pcb.DirectoryTableBase;
    }
}


uint64_t WindowsMonitor::getModuleSizeFromCfg(const std::string &module) const
{

    ModuleSizeMap::const_iterator it;
    it = m_ModuleInfo.find(module);
    if (it == m_ModuleInfo.end()) {
        return 0;
    }
    return (*it).second;
}

bool WindowsMonitor::getCurrentStack(S2EExecutionState *state, uint64_t *base, uint64_t *size)
{
    if (!isKernelAddress(state->getPc())) {
        assert(false && "User-mode stack retrieval not implemented");
    }

    uint64_t pThread = getCurrentThread(state);
    if (!pThread) {
        return false;
    }

    s2e::windows::KTHREAD32 kThread;
    if (!state->readMemoryConcrete(pThread, &kThread, sizeof(kThread))) {
        return false;
    }


    if (base) {
        *base = kThread.StackLimit;
    }
    if (size) {
        *size = kThread.InitialStack - kThread.StackLimit;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////

WindowsMonitorState::WindowsMonitorState()
{
    m_CurrentPid = -1;
}

WindowsMonitorState::~WindowsMonitorState()
{

}

WindowsMonitorState* WindowsMonitorState::clone() const
{
    return new WindowsMonitorState(*this);
}

PluginState *WindowsMonitorState::factory(Plugin *p, S2EExecutionState *state)
{
    return new WindowsMonitorState();
}
