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

#ifndef _WINDOWS_PLUGIN_H_

#define _WINDOWS_PLUGIN_H_

#include <s2e/Plugins/ModuleDescriptor.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>

#include "WindowsImage.h"

#include <inttypes.h>
#include <set>
#include <map>


namespace s2e {
namespace plugins {

class WindowsUmInterceptor;
class WindowsKmInterceptor;
class WindowsSpy;

typedef std::set<uint64_t> PidSet;
typedef std::map<std::string, uint64_t> ModuleSizeMap;

class WindowsMonitor:public OSMonitor
{
    S2E_PLUGIN

public:

    //Do not change the order of the constants
    typedef enum EWinVer {
        XPSP2=0,
        XPSP3=1,
        XPSP2_CHK=2,
        XPSP3_CHK=3,
        SRV2008SP2=4,
        MAXVER
    }EWinVer;

private:

    //These are the keys to specify in the configuration file
    static const char *s_windowsKeys[];

    //These are user-friendly strings displayed to the user
    static const char *s_windowsStrings[];

    //Specifies whether or not the given version is checked
    static bool s_checkedMap[];

    static unsigned s_pointerSize[];
    static uint64_t s_kernelNativeBase[];

    static uint64_t s_ntdllNativeBase[];
    static uint64_t s_ntdllLoadBase[];
    static uint64_t s_ntdllSize[];

    static uint64_t s_driverLoadPc[];
    static uint64_t s_driverDeletePc[];
    static uint64_t s_kdDbgDataBlock[];

    static uint64_t s_panicPc1[];
    static uint64_t s_panicPc2[];
    static uint64_t s_panicPc3[];

    static uint64_t s_ntTerminateProc[];
    static uint64_t s_ntKeInitThread[];
    static uint64_t s_ntKeTerminateThread[];

    static uint64_t s_sysServicePc[];

    static uint64_t s_psProcListPtr[];

    static uint64_t s_ldrpCall[];
    static uint64_t s_dllUnloadPc[];

    static uint64_t s_offEprocAllThreads[];
    static uint64_t s_offEthreadLink[];


    EWinVer m_Version;
    bool m_UserMode, m_KernelMode;

    uint64_t m_KernelBase;

    bool m_MonitorModuleLoad;
    bool m_MonitorModuleUnload;
    bool m_MonitorProcessUnload;
    bool m_monitorThreads;

    bool m_FirstTime;
    bool m_TrackPidSet;
    PidSet m_PidSet;

    //Dynamically-computed addresses. Required for ALSR-enabled Windows versions
    uint32_t m_pKPCRAddr;
    uint32_t m_pKPRCBAddr;
    windows::DBGKD_GET_VERSION64 m_kdVersion;
    windows::KPRCB32 m_kprcb;

    WindowsUmInterceptor *m_UserModeInterceptor;
    WindowsKmInterceptor *m_KernelModeInterceptor;

    sigc::connection m_SyscallConnection;

    ModuleSizeMap m_ModuleInfo;

    void readModuleCfg();
    void InitializeAddresses(S2EExecutionState *state);

    void slotTranslateInstructionStart(ExecutionSignal *signal,
        S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t pc);

    void onPageDirectoryChange(S2EExecutionState *state, uint64_t previous, uint64_t current);

    void slotMonitorProcessSwitch(S2EExecutionState *state, uint64_t pc);
    void slotUmCatchModuleLoad(S2EExecutionState *state, uint64_t pc);
    void slotUmCatchModuleUnload(S2EExecutionState *state, uint64_t pc);
    void slotUmCatchProcessTermination(S2EExecutionState *state, uint64_t pc);

    void slotKmUpdateModuleList(S2EExecutionState *state, uint64_t pc);
    void slotKmModuleLoad(S2EExecutionState *state, uint64_t pc);
    void slotKmModuleUnload(S2EExecutionState *state, uint64_t pc);

    void slotKmThreadInit(S2EExecutionState *state, uint64_t pc);
    void slotKmThreadExit(S2EExecutionState *state, uint64_t pc);

    void notifyLoadForAllThreads(S2EExecutionState *state);
public:
    WindowsMonitor(S2E* s2e): OSMonitor(s2e) {}
    virtual ~WindowsMonitor();
    void initialize();




    virtual bool getImports(S2EExecutionState *s, const ModuleDescriptor &desc, Imports &I);
    virtual bool getExports(S2EExecutionState *s, const ModuleDescriptor &desc, Exports &E);

    uint64_t GetUserAddressSpaceSize() const;
    uint64_t GetKernelStart() const;
    uint64_t GetKernelLoadBase() const;
    uint64_t GetLdrpCallInitRoutine() const;
    uint64_t GetNtTerminateProcessEProcessPoint() const;
    uint64_t GetDllUnloadPc() const;
    uint64_t GetDeleteDriverPc() const;
    uint64_t GetDriverLoadPc() const;
    uint64_t GetSystemServicePc() const;
    uint64_t GetPsActiveProcessListPtr() const;
    uint64_t GetKeInitThread() const;
    uint64_t GetKeTerminateThread() const;
    uint64_t GetKdDebuggerDataBlock() const;
    uint64_t getCurrentProcess(S2EExecutionState *state);
    uint64_t getCurrentThread(S2EExecutionState *state);
    uint64_t getFirstThread(S2EExecutionState *state, uint64_t eprocess);
    uint64_t getNextThread(S2EExecutionState *state, uint64_t thread);
    uint64_t getPeb(S2EExecutionState *state, uint64_t eprocess);
    uint64_t getTibAddress(S2EExecutionState *state);
    bool     getTib(S2EExecutionState *state, s2e::windows::NT_TIB32 *tib);
    uint64_t getProcessFromLink(uint64_t pItem);
    uint64_t getDirectoryTableBase(S2EExecutionState *state, uint64_t pProcessEntry);

    uint64_t getModuleSizeFromCfg(const std::string &module) const;

    windows::KPRCB32 getKprcb() const { return m_kprcb; }
    uint64_t getKpcrbAddress() const { return m_pKPRCBAddr; }

    uint64_t getBuildNumber() const {
        return m_kdVersion.MinorVersion;
    }

    bool CheckPanic(uint64_t eip) const;
    unsigned GetPointerSize() const;

    EWinVer GetVersion() const {
        return m_Version;
    }

    WindowsSpy *getSpy() const {
        return NULL;
    }

    bool isCheckedBuild() const {
        return s_checkedMap[m_Version];
    }

    virtual bool isKernelAddress(uint64_t pc) const;
    virtual uint64_t getPid(S2EExecutionState *s, uint64_t pc);
    virtual bool getCurrentStack(S2EExecutionState *s, uint64_t *base, uint64_t *size);

    bool getThreadStack(S2EExecutionState *state, uint64_t pThread, uint64_t *base, uint64_t *size);
    bool getDpcStack(S2EExecutionState *state, uint64_t *base, uint64_t *size);

    bool getThreadDescriptor(S2EExecutionState *state,
                             uint64_t pThread,
                             ThreadDescriptor &threadDescriptor);

    const windows::DBGKD_GET_VERSION64 &getVersionBlock() const {
        return m_kdVersion;
    }

    bool getAllProcesses(S2EExecutionState *state, std::vector<uint64_t> &pEProcess);
    bool getAllThreads(S2EExecutionState *state, uint64_t process, std::vector<uint64_t> &pEThread);
};

class WindowsMonitorState:public PluginState
{
private:
    uint64_t m_CurrentPid;

public:
    WindowsMonitorState();
    virtual ~WindowsMonitorState();
    virtual WindowsMonitorState* clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *state);

    friend class WindowsMonitor;
};

} // namespace plugins
} // namespace s2e


#endif
