#define NDEBUG

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

    m_CheckedBuild = s2e()->getConfig()->getBool(getConfigKey() + ".checked");
    if (m_CheckedBuild) {
        s2e()->getWarningsStream() << "You specified a CHECKED build of Windows. Only kernel-mode interceptor " <<
                "is properly supported for now" << std::endl;
    }

    switch(m_Version) {
        case SP2:
            assert(false && "SP2 support not implemented");
            break;

        case SP3:
            m_NtkernelBase = m_CheckedBuild ? 0x80a02000 : 0x804d7000;
            break;
        default:
            assert(false);
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

void WindowsMonitor::slotTranslateInstructionStart(ExecutionSignal *signal,
                                                   S2EExecutionState *state,
                                                   TranslationBlock *tb,
                                                   uint64_t pc)
{
    //XXX: on resume vm snapshot, the init routines may not be called.
    //However, when it is called, it will automatically scan all loaded modules.
    if(m_UserMode) {
        if (m_FirstTime) {
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

        /*if (pc == GetSystemServicePc() && m_FirstTime) {
            m_SyscallConnection = signal->connect(sigc::mem_fun(*this, &WindowsMonitor::slotKmUpdateModuleList));
        }*/
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
                ntdll.NativeBase = 0x7c900000;
                ntdll.LoadBase = 0x7c900000;
                ntdll.Size = 0x7a000;
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
    s2e()->getDebugStream() << "Detected task switch at 0x" << oldpc << std::endl;

    return true;

failure:
    //s2e()->getDebugStream() << "Could not read 0x" << std::hex << oldpc << " in isTaskSwitch" << std::dec << std::endl;
    return false;
}

uint64_t WindowsMonitor::GetDriverLoadPc() const
{
    if (m_CheckedBuild) {
        switch(m_Version) {
            case SP2: assert(false && "Not implemented");
            case SP3: return 0x0053d5d6 - 0x400000 + m_NtkernelBase; //0x80B3F5D6
            default: return 0;
        }

    }else {
        switch(m_Version) {
            case SP2: assert(false && "Not implemented");
            case SP3: return 0x004cc99a - 0x400000 + m_NtkernelBase; //0x805A399A
            default: return 0;
        }
    }

    assert(false);
    return 0;
}

uint64_t WindowsMonitor::GetKdDebuggerDataBlock() const
{
    if (m_CheckedBuild) {
        switch(m_Version) {
            case SP2: assert(false && "Not implemented");
            case SP3: return 0x004ec3f0 - 0x400000 + m_NtkernelBase;
            default: return 0;
        }

    }else {
        switch(m_Version) {
            case SP2: assert(false && "Not implemented");
            case SP3: return 0x00475DE0 - 0x400000 + m_NtkernelBase;
            default: return 0;
        }
    }

    assert(false);
    return 0;
}

bool WindowsMonitor::CheckPanic(uint64_t eip) const
{
    if (m_CheckedBuild) {
        switch(m_Version) {
        case SP2:
            assert(false && "Not implemented");
            eip = eip - m_NtkernelBase + 0x400000;
            return (eip == 0x0045B7BA  || eip == 0x0045C2DF || eip == 0x0045C303);
            break;
        case SP3:
            eip = eip - m_NtkernelBase + 0x400000;
            return (eip == 0x42f478  || eip == 0x42ff44 || eip == 0x42ff62);
            break;
        default:
            return false;
        }
    }else {
        switch(m_Version) {
        case SP2:
            assert(false && "Not implemented");
            eip = eip - m_NtkernelBase + 0x400000;
            return (eip == 0x0045B7BA  || eip == 0x0045C2DF || eip == 0x0045C303);
            break;
        case SP3:
            eip = eip - m_NtkernelBase + 0x400000;
            return (eip == 0x0045BCAA  || eip == 0x0045C7CD || eip == 0x0045C7F3);
            break;
        default:
            return false;
        }
    }

    return false;
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
    if (m_CheckedBuild) {
        switch(m_Version) {
            case SP2: assert (false && "Not implemented");
                //XXX: test this
            case SP3: return (0x5dab73 - 0x400000 + m_NtkernelBase);
        }

    }else {
        switch(m_Version) {
            case SP2: assert (false && "Not implemented");
            case SP3: return (0x004ab3c8 - 0x400000 + m_NtkernelBase);
        }
    }
    assert(false && "Unknown OS version\n");
    return 0;
}

uint64_t WindowsMonitor::GetDeleteDriverPc() const
{
    if (m_CheckedBuild) {
        switch(m_Version) {
            case SP2: assert (false && "Not implemented");
            case SP3: return (0x540a72 - 0x400000 + m_NtkernelBase);
        }
    }else {
        switch(m_Version) {
            case SP2: assert (false && "Not implemented");
            case SP3: return (0x004EB33F - 0x400000 + m_NtkernelBase);
        }
    }
    assert(false && "Unknown OS version\n");
    return 0;
}

uint64_t WindowsMonitor::GetSystemServicePc() const
{
    if (m_CheckedBuild) {
        switch(m_Version) {
            case SP2: assert (false && "Not implemented");
            case SP3: return (0x4dca05 - 0x400000 + m_NtkernelBase);
        }

    }else {
        switch(m_Version) {
            case SP2: assert (false && "Not implemented");
            case SP3: return (0x00407631 - 0x400000 + m_NtkernelBase);
        }
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
    if (m_CheckedBuild) {
        switch(m_Version) {
            case SP2: assert (false && "Not implemented");
            case SP3: return (0x5102b8 - 0x400000 + m_NtkernelBase);
        }
    } else {
        switch(m_Version) {
            case SP2: assert (false && "Not implemented");
            case SP3: return (0x0048A358 - 0x400000 + m_NtkernelBase);
        }
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
    if (pc >= GetKernelStart()) {
        return 0;
    }
    return s->getPid();
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
