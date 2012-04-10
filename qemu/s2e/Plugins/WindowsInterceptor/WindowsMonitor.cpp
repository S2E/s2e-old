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

#include <s2e/Plugins/WindowsApi/Ntddk.h>

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

/*                                             XPSP2         XPSP3        XPSP2-CHK   XPSP3-CHK   SRV2008SP2 */
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

//Points to the instruction after which the kernel-mode stack in KTHREAD is properly initialized
uint64_t WindowsMonitor::s_ntKeInitThread[] =  {0x00000000, 0x004b75fc, 0x00000000, 0x00000000, 0x00000000};

//Points to the start of KeTerminateThread (this function terminates the current thread)
uint64_t WindowsMonitor::s_ntKeTerminateThread[] = {0x00000000, 0x004214c9, 0x00000000, 0x00000000, 0x00000000};

uint64_t WindowsMonitor::s_sysServicePc[] =    {0x00000000, 0x00407631, 0x00000000, 0x004dca05, 0x0045777E};

uint64_t WindowsMonitor::s_psProcListPtr[] =   {0x00000000, 0x0048A358, 0x00000000, 0x005102b8, 0x00504150};

uint64_t WindowsMonitor::s_ldrpCall[] =        {0x7C901193, 0x7C901176, 0x00000000, 0x00000000, 0x77F11698};
uint64_t WindowsMonitor::s_dllUnloadPc[] =     {0x00000000, 0x7c91e12a, 0x00000000, 0x00000000, 0x77F0BB58};

//Offset of the thread list head in EPROCESS
uint64_t WindowsMonitor::s_offEprocAllThreads[] =     {0x0, 0x190, 0x0, 0x0, 0x0};
//Offset of the thread list link in ETHREAD
uint64_t WindowsMonitor::s_offEthreadLink[] =         {0x0, 0x22c, 0x0, 0x0, 0x0};


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
    m_monitorThreads = s2e()->getConfig()->getBool(getConfigKey() + ".monitorThreads", true);


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

    //XXX: Warn about some unsupported features
    if (m_Version != XPSP3 && m_monitorThreads) {
        s2e()->getWarningsStream() << "WindowsMonitor does not support threads for the chosen OS version.\n"
                                   << "Please use monitorThreads=false in the configuration file\n"
                                   << "Plugins that depend on this feature will not work.\n";
        exit(-1);
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

    s2e()->getCorePlugin()->onPageDirectoryChange.connect(
        sigc::mem_fun(*this, &WindowsMonitor::onPageDirectoryChange));

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

//XXX: This may slowdown translation as it is called on every instruction
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
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotUmCatchModuleLoad));
        }else if (pc == GetNtTerminateProcessEProcessPoint() && m_MonitorProcessUnload) {
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotUmCatchProcessTermination));
        }else if (pc == GetDllUnloadPc() && m_MonitorModuleUnload) {
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotUmCatchModuleUnload));
        }
    }

    if(m_KernelMode) {
        //XXX: a module load can be notified twice if it was being loaded while the snapshot was saved.
        if (m_FirstTime) {
            slotKmUpdateModuleList(state, pc);
            notifyLoadForAllThreads(state);
        }

        if (pc == GetDriverLoadPc()) {
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotKmModuleLoad));
        }else if (pc == GetDeleteDriverPc()) {
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotKmModuleUnload));
        }else if (m_monitorThreads && pc == GetKeInitThread()) {
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotKmThreadInit));
        }else if (m_monitorThreads && pc == GetKeTerminateThread()) {
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotKmThreadExit));
        }
    }

    m_FirstTime = false;
}


void WindowsMonitor::onPageDirectoryChange(S2EExecutionState *state, uint64_t previous, uint64_t current)
{
    DECLARE_PLUGINSTATE(WindowsMonitorState, state);

    if (m_PidSet.size() == 0) {
        m_TrackPidSet = false;
        return;
    }

    if (previous != current) {
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
    s2e()->getDebugStream(state) << "User mode module load at " << hexval(pc) << '\n';
    m_UserModeInterceptor->CatchModuleLoad(state);
}

void WindowsMonitor::slotUmCatchModuleUnload(S2EExecutionState *state, uint64_t pc)
{
    s2e()->getDebugStream(state) << "User mode module unload at " << hexval(pc) << '\n';
    m_UserModeInterceptor->CatchModuleUnload(state);
}

void WindowsMonitor::slotUmCatchProcessTermination(S2EExecutionState *state, uint64_t pc)
{
    s2e()->getDebugStream(state) << "Caught process termination\n";
    m_UserModeInterceptor->CatchProcessTermination(state);
}

void WindowsMonitor::slotKmModuleLoad(S2EExecutionState *state, uint64_t pc)
{
    s2e()->getDebugStream(state) << "Kernel mode module load at " << hexval(pc) << '\n';
    m_KernelModeInterceptor->CatchModuleLoad(state);
}

void WindowsMonitor::slotKmModuleUnload(S2EExecutionState *state, uint64_t pc)
{
    s2e()->getDebugStream(state) << "Kernel mode module unload at " << hexval(pc) << '\n';
    m_KernelModeInterceptor->CatchModuleUnload(state);
}

void WindowsMonitor::slotKmThreadInit(S2EExecutionState *state, uint64_t pc)
{
    s2e()->getDebugStream() << "WindowsMonitor: creating kernel-mode thread\n";

    uint32_t pThread;
    assert(m_Version == XPSP3 && "Unsupported OS version");

    //XXX: Fix assumption about ESI register
    if (!state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ESI]), &pThread, sizeof(pThread))) {
        return;
    }

    ThreadDescriptor threadDescriptor;
    bool res = getThreadDescriptor(state, pThread, threadDescriptor);
    if (!res) {
        return;
    }

    onThreadCreate.emit(state, threadDescriptor);
}

void WindowsMonitor::slotKmThreadExit(S2EExecutionState *state, uint64_t pc)
{
    uint64_t pThread = getCurrentThread(state);

    ThreadDescriptor threadDescriptor;
    bool res = getThreadDescriptor(state, pThread, threadDescriptor);
    if (!res) {
        return;
    }

    s2e()->getDebugStream() << "WindowsMonitor: terminating kernel-mode thread stack="
                            << hexval(threadDescriptor.KernelStackBottom)
                            << " size=" << hexval(threadDescriptor.KernelStackSize) << "\n";
    onThreadExit.emit(state, threadDescriptor);
}

/**
 *  Scans the list of kernel modules and registers each entry.
 *  This is useful in case the VM snapshot was resumed (and all
 *  the modules are already loaded, but not registered with S2E).
 */
void WindowsMonitor::slotKmUpdateModuleList(S2EExecutionState *state, uint64_t pc)
{
    s2e()->getDebugStream(state) << "Kernel mode module update at " << hexval(pc) << '\n';
    if (m_KernelModeInterceptor->ReadModuleList(state)) {
        //List updated, unregister
        m_SyscallConnection.disconnect();
        m_FirstTime = false;
    }
}

bool WindowsMonitor::getAllProcesses(S2EExecutionState *state, std::vector<uint64_t> &pEProcess)
{
    windows::LIST_ENTRY32 ListHead;
    uint64_t ActiveProcessList = GetPsActiveProcessListPtr();
    uint64_t pItem;

    //Read the head of the list
    if (!state->readMemoryConcrete(ActiveProcessList, &ListHead, sizeof(ListHead))) {
        return false;
    }

    //Check for empty list
    if (ListHead.Flink == ActiveProcessList) {
        return false;
    }

    for (pItem = ListHead.Flink; pItem != ActiveProcessList; pItem = ListHead.Flink) {
        if (!state->readMemoryConcrete(pItem, &ListHead, sizeof(ListHead))) {
            return false;
        }

        uint32_t pProcessEntry = getProcessFromLink(pItem);
        pEProcess.push_back(pProcessEntry);
    }
    return true;
}

bool WindowsMonitor::getAllThreads(S2EExecutionState *state, uint64_t process, std::vector<uint64_t> &pEThread)
{
    uint64_t headOffset = s_offEprocAllThreads[m_Version];
    uint64_t linkOffset = s_offEthreadLink[m_Version];

    assert(linkOffset && headOffset && "Not implemented yet\n");

    uint64_t ThreadList = process + headOffset;

    windows::LIST_ENTRY32 ListHead;
    //Read the head of the list
    if (!state->readMemoryConcrete(ThreadList, &ListHead, sizeof(ListHead))) {
        return false;
    }

    //Check for empty list
    if (ListHead.Flink == ThreadList) {
        return false;
    }

    for (uint32_t pItem = ListHead.Flink; pItem != ThreadList; pItem = ListHead.Flink) {
        if (!state->readMemoryConcrete(pItem, &ListHead, sizeof(ListHead))) {
            return false;
        }

        pEThread.push_back(pItem - linkOffset);
    }
    return true;

}

void WindowsMonitor::notifyLoadForAllThreads(S2EExecutionState *state)
{
    std::vector<uint64_t> processes, threads;
    if (!getAllProcesses(state, processes)) {
        return;
    }

    foreach2(it, processes.begin(), processes.end()) {
        uint64_t eprocess = *it;

        getAllThreads(state, eprocess, threads);

        foreach2(tit, threads.begin(), threads.end()) {
            ThreadDescriptor threadDescriptor;
            bool res = getThreadDescriptor(state, *tit, threadDescriptor);
            if (!res) {
                return;
            }

            //XXX: some thread descriptors seem broken
            if (threadDescriptor.KernelStackBottom &&
                    threadDescriptor.KernelStackSize < 0x10000) {
                onThreadCreate.emit(state, threadDescriptor);
            }
        }
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


uint64_t WindowsMonitor::GetDriverLoadPc() const
{
    assert(s_driverLoadPc[m_Version]  && "WindowsMonitor does not support this OS version yet");
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_driverLoadPc[m_Version] + offset;
}

uint64_t WindowsMonitor::GetKdDebuggerDataBlock() const
{
    assert(s_kdDbgDataBlock[m_Version] && "WindowsMonitor does not support this OS version yet");
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return (s_kdDbgDataBlock[m_Version] + offset);
}

bool WindowsMonitor::CheckPanic(uint64_t eip) const
{
    assert(s_panicPc1[m_Version] && s_panicPc2[m_Version] && s_panicPc3[m_Version]
           && "WindowsMonitor does not support this OS version yet");
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
    assert(s_ldrpCall[m_Version] && "WindowsMonitor does not support this OS version yet");
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
    assert(s_ntTerminateProc[m_Version] && "WindowsMonitor does not support this OS version yet");
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_ntTerminateProc[m_Version] + offset;
}

uint64_t WindowsMonitor::GetDeleteDriverPc() const
{
    assert(s_driverDeletePc[m_Version] && "WindowsMonitor does not support this OS version yet");
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_driverDeletePc[m_Version] + offset;
}

uint64_t WindowsMonitor::GetSystemServicePc() const
{
    assert(s_sysServicePc[m_Version] && "WindowsMonitor does not support this OS version yet");
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_sysServicePc[m_Version] + offset;
}

uint64_t WindowsMonitor::GetDllUnloadPc() const
{
    assert(s_dllUnloadPc[m_Version] && "WindowsMonitor does not support this OS version yet");
    uint64_t offset = s_ntdllLoadBase[m_Version] - s_ntdllNativeBase[m_Version];
    return s_dllUnloadPc[m_Version] + offset;
}

uint64_t WindowsMonitor::GetPsActiveProcessListPtr() const
{
    assert(s_psProcListPtr[m_Version] && "WindowsMonitor does not support this OS version yet");
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_psProcListPtr[m_Version] + offset;
}

uint64_t WindowsMonitor::GetKeInitThread() const
{
    assert(s_ntKeInitThread[m_Version] && "WindowsMonitor does not support this OS version yet");
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_ntKeInitThread[m_Version] + offset;
}

uint64_t WindowsMonitor::GetKeTerminateThread() const
{
    assert(s_ntKeTerminateThread[m_Version] && "WindowsMonitor does not support this OS version yet");
    uint64_t offset = GetKernelLoadBase() - s_kernelNativeBase[m_Version];
    return s_ntKeTerminateThread[m_Version] + offset;
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

uint64_t WindowsMonitor::getFirstThread(S2EExecutionState *state, uint64_t eprocess)
{
    uint64_t threadListEntryOffset;

    if (m_kdVersion.MinorVersion >= BUILD_LONGHORN) {
        assert(false && "Not implemented yet");
    }else {
        threadListEntryOffset = windows::EPROCESS32_THREADLISTHEAD_OFFSET_XP;
    }

    windows::LIST_ENTRY32 nextThread;
    if (!state->readMemoryConcrete(eprocess + threadListEntryOffset, &nextThread, sizeof(nextThread))) {
        return 0;
    }

    return nextThread.Flink - threadListEntryOffset;
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

//Returns true if the current stack pointer falls within the DPC stack
//of the current processor
bool WindowsMonitor::getDpcStack(S2EExecutionState *state, uint64_t *base, uint64_t *size)
{
    //XXX: Hack to get DPC stacks
    uint32_t sp = state->getSp();
    uint32_t dpcStackPtr = m_pKPRCBAddr + windows::KPRCB32_DPC_STACK_OFFSET;
    uint32_t dpcStack;

    if (state->readMemoryConcrete(dpcStackPtr, &dpcStack, sizeof(dpcStack))) {
        dpcStack -= 0x3000;
        s2e()->getDebugStream() << "WindowsMonitor esp=" << hexval(sp) << " dpc=" << hexval(dpcStack) << '\n';
        if (sp >= dpcStack && sp < (dpcStack + 0x3000)) {
            if (base) {
                *base = dpcStack;
            }
            if (size) {
                *size = 0x3000;
            }
            return true;
        }
    }

    return false;
}

bool WindowsMonitor::getThreadStack(S2EExecutionState *state,
                                    uint64_t pThread,
                                    uint64_t *base, uint64_t *size)
{
    if (!isKernelAddress(state->getPc())) {
        assert(false && "User-mode stack retrieval not implemented");
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

bool WindowsMonitor::getCurrentStack(S2EExecutionState *state, uint64_t *base, uint64_t *size)
{
    if (getDpcStack(state, base, size)) {
        return true;
    }

    uint64_t pThread = getCurrentThread(state);
    if (!pThread) {
        return false;
    }

    return getThreadStack(state, pThread, base, size);
}

//XXX: Does not work for user-mode for now.
bool WindowsMonitor::getThreadDescriptor(S2EExecutionState *state,
                                         uint64_t pThread,
                                         ThreadDescriptor &threadDescriptor)
{
    uint64_t base = 0, size = 0;

    if (!getThreadStack(state, pThread, &base, &size)) {
        return false;
    }

    threadDescriptor.KernelMode = true;
    threadDescriptor.KernelStackBottom = base;
    threadDescriptor.KernelStackSize = size;

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
