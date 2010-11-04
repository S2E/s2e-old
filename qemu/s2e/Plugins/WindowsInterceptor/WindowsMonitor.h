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
 * Main authors: Vitaly Chipounov, Volodymyr Kuznetsov.
 * All S2E contributors are listed in the S2E-AUTHORS file.
 *
 */

#ifndef _WINDOWS_PLUGIN_H_

#define _WINDOWS_PLUGIN_H_

#include <s2e/Plugins/ModuleDescriptor.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>

//#include "WindowsSpy.h"

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
  typedef enum EWinVer {
    SP2, SP3
  }EWinVer;

private:
    EWinVer m_Version;
    bool m_UserMode, m_KernelMode;
    unsigned m_PointerSize;
    uint64_t m_KernelBase;
    bool m_CheckedBuild;

    bool m_MonitorModuleLoad;
    bool m_MonitorModuleUnload;
    bool m_MonitorProcessUnload;

    bool m_FirstTime;
    bool m_TrackPidSet;
    PidSet m_PidSet;

    uint32_t m_NtkernelBase;

    WindowsUmInterceptor *m_UserModeInterceptor;
    WindowsKmInterceptor *m_KernelModeInterceptor;

    sigc::connection m_SyscallConnection;

    ModuleSizeMap m_ModuleInfo;

    void readModuleCfg();
public:
    WindowsMonitor(S2E* s2e): OSMonitor(s2e) {}
    virtual ~WindowsMonitor();
    void initialize();

    void slotTranslateInstructionStart(ExecutionSignal *signal,
        S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t pc);

    void slotTranslateInstructionEnd(ExecutionSignal *signal,
        S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t pc);

    void slotMonitorProcessSwitch(S2EExecutionState *state, uint64_t pc);
    void slotUmCatchModuleLoad(S2EExecutionState *state, uint64_t pc);
    void slotUmCatchModuleUnload(S2EExecutionState *state, uint64_t pc);
    void slotUmCatchProcessTermination(S2EExecutionState *state, uint64_t pc);

    void slotKmUpdateModuleList(S2EExecutionState *state, uint64_t pc);
    void slotKmModuleLoad(S2EExecutionState *state, uint64_t pc);
    void slotKmModuleUnload(S2EExecutionState *state, uint64_t pc);

    virtual bool getImports(S2EExecutionState *s, const ModuleDescriptor &desc, Imports &I);
    virtual bool getExports(S2EExecutionState *s, const ModuleDescriptor &desc, Exports &E);

    bool isTaskSwitch(S2EExecutionState *state, uint64_t pc);

    uint64_t GetUserAddressSpaceSize() const;
    uint64_t GetKernelStart() const;
    uint64_t GetLdrpCallInitRoutine() const;
    uint64_t GetNtTerminateProcessEProcessPoint() const;
    uint64_t GetDllUnloadPc() const;
    uint64_t GetDeleteDriverPc() const;
    uint64_t GetDriverLoadPc() const;
    uint64_t GetSystemServicePc() const;
    uint64_t GetPsActiveProcessListPtr() const;
    uint64_t GetKdDebuggerDataBlock() const;

    uint64_t getModuleSizeFromCfg(const std::string &module) const;

    bool CheckPanic(uint64_t eip) const;
    unsigned GetPointerSize() const;

    EWinVer GetVersion() const {
        return m_Version;
    }

    WindowsSpy *getSpy() const {
        return NULL;
    }

    bool isCheckedBuild() const {
        return m_CheckedBuild;
    }

    virtual bool isKernelAddress(uint64_t pc) const;
    virtual uint64_t getPid(S2EExecutionState *s, uint64_t pc);

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
