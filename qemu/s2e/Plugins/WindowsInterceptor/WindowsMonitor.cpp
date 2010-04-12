#define NDEBUG

// XXX: qemu stuff should be included before anything from KLEE or LLVM !
extern "C" {
#include "config.h"
//#include "cpu.h"
//#include "exec-all.h"
#include "qemu-common.h"
}

#include <s2e/s2e.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>
#include "WindowsMonitor.h"
#include "WindowsImage.h"
#include "UserModeInterceptor.h"
#include "KernelModeInterceptor.h"

#include <string>
#include <cstring>
#include <iostream>
#include <assert.h>

using namespace std;

using namespace s2e;
using namespace s2e::plugins;

S2E_DEFINE_PLUGIN(WindowsMonitor, "Plugin for monitoring Windows kernel/user-mode events", "Interceptor");

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

    if (!strcasecmp(Version.c_str(), "SP2")) {
        m_Version = WindowsMonitor::SP2;
        m_PointerSize = 4;
    }else if (!strcasecmp(Version.c_str(), "SP3")) {
        m_Version = WindowsMonitor::SP3;
        m_PointerSize = 4;
    }else {
        std::cout << "Unsupported of invalid Windows version " << Version << std::endl;
        exit(-1);
    }

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
}

void WindowsMonitor::slotTranslateInstructionStart(ExecutionSignal *signal, 
                                                   S2EExecutionState *state,
                                                   uint64_t pc)
{
    if(m_UserMode) {
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
        if (pc == GetSystemServicePc() && m_FirstTime) {
            m_SyscallConnection = signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotKmUpdateModuleList));
        }if (pc == GetDriverLoadPc()) {
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotKmModuleLoad));
        }else if (pc == GetDeleteDriverPc()) {
            signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotKmModuleUnload));
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
    CPUState *cpuState = (CPUState *)s->getCpuState();
    if (desc.Pid && cpuState->cr[3] != desc.Pid) {
        return false;
    }

    WindowsImage Img(desc.LoadBase);
    I = Img.GetImports();
    return true;
}

bool WindowsMonitor::getExports(S2EExecutionState *s, const ModuleDescriptor &desc, Exports &E)
{
    CPUState *cpuState = (CPUState *)s->getCpuState();
    if (desc.Pid && cpuState->cr[3] != desc.Pid) {
        return false;
    }

    WindowsImage Img(desc.LoadBase);
    E = Img.GetExports();
    return true;
}


uint64_t WindowsMonitor::GetDriverLoadPc() const
{
    switch(m_Version) {
    case SP2: assert(false && "Not implemented"); 
        //return eip >= 0x805A078C && eip <= 0x805A0796;
    case SP3: return 0x004cc99a - 0x400000 + 0x804d7000;
    default: return 0;
    }

    assert(false);
    return 0;
}

bool WindowsMonitor::CheckPanic(uint64_t eip) const
{
    eip -= m_KernelBase;

    switch(m_Version) {
    case SP2: return eip == 0x0045B7BA  || eip == 0x0045C2DF || eip == 0x0045C303;
    case SP3: return eip == 0x0045BCAA  || eip == 0x0045C7CD || eip == 0x0045C7F3;
    default: return false;
    }
    return true;
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
    switch(m_Version) {
    case SP2: return 0x7C901193;
    case SP3: return 0x7C901176;
    }
    assert(false && "Unknown OS version\n");
    return 0;
}

unsigned WindowsMonitor::GetPointerSize() const
{
    return m_PointerSize;
}



uint64_t WindowsMonitor::GetNtTerminateProcessEProcessPoint() const
{
    switch(m_Version) {
    case SP2: assert (false && "Not implemented");
    case SP3: return (0x004ab3c8 - 0x400000 + 0x804d7000);
    }
    assert(false && "Unknown OS version\n");
    return 0;
}

uint64_t WindowsMonitor::GetDeleteDriverPc() const
{
    switch(m_Version) {
    case SP2: assert (false && "Not implemented");
    case SP3: return (0x004EB33F - 0x400000 + 0x804d7000);
    }
    assert(false && "Unknown OS version\n");
    return 0;
}

uint64_t WindowsMonitor::GetSystemServicePc() const
{
    switch(m_Version) {
    case SP2: assert (false && "Not implemented");
    case SP3: return (0x00407631 - 0x400000 + 0x804d7000);
    }
    assert(false && "Unknown OS version\n");
    return 0;
}

uint64_t WindowsMonitor::GetDllUnloadPc() const
{
    switch(m_Version) {
    case SP2: assert (false && "Not implemented");
    case SP3: return 0x7c91e12a; //0x7c91dfb3; //LdrUnloadDll
    }
    assert(false && "Unknown OS version\n");
    return 0;
}

uint64_t WindowsMonitor::GetPsActiveProcessListPtr() const
{
    switch(m_Version) {
    case SP2: assert (false && "Not implemented");
    case SP3: return (0x0048A358 - 0x400000 + 0x804d7000);
    }
    assert(false && "Unknown OS version\n");
    return 0;
}

bool WindowsMonitor::isKernelAddress(uint64_t pc) const
{
    //XXX: deal with large address space awareness
    return pc >= GetKernelStart();
}

uint64_t WindowsMonitor::getPid(S2EExecutionState *s, uint64_t pc)
{
    CPUState *cpuState = (CPUState *)s->getCpuState();
    if (pc >= GetKernelStart()) {
        return 0;
    }
    return cpuState->cr[3];
}
