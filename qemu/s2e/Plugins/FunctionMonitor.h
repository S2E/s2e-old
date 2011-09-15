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

#ifndef S2E_PLUGINS_FUNCTIONMONITOR_H
#define S2E_PLUGINS_FUNCTIONMONITOR_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Plugins/OSMonitor.h>

#include <tr1/unordered_map>

namespace s2e {
namespace plugins {

class FunctionMonitorState;

class FunctionMonitor : public Plugin
{
    S2E_PLUGIN
public:
    FunctionMonitor(S2E* s2e): Plugin(s2e) {}

    typedef sigc::signal<void, S2EExecutionState*> ReturnSignal;
    typedef sigc::signal<void, S2EExecutionState*, FunctionMonitorState*> CallSignal;

    void initialize();
    
    CallSignal* getCallSignal(
            S2EExecutionState *state,
            uint64_t eip, uint64_t cr3 = 0);

    void registerReturnSignal(S2EExecutionState *state, FunctionMonitor::ReturnSignal &sig);

    void eraseSp(S2EExecutionState *state, uint64_t pc);
    void disconnect(S2EExecutionState *state, const ModuleDescriptor &desc);
protected:
    void slotTranslateBlockEnd(ExecutionSignal*, S2EExecutionState *state,
                               TranslationBlock *tb, uint64_t pc,
                               bool, uint64_t);

    void slotTranslateJumpStart(ExecutionSignal *signal,
                                S2EExecutionState *state,
                                TranslationBlock*,
                                uint64_t, int jump_type);

    void slotCall(S2EExecutionState* state, uint64_t pc);
    void slotRet(S2EExecutionState* state, uint64_t pc);

    void slotTraceCall(S2EExecutionState *state, FunctionMonitorState *fns);
    void slotTraceRet(S2EExecutionState *state, int f);

protected:
    OSMonitor *m_monitor;

    friend class FunctionMonitorState;

};

class FunctionMonitorState : public PluginState
{

    struct CallDescriptor {
        uint64_t cr3;
        // TODO: add sourceModuleID and targetModuleID
        FunctionMonitor::CallSignal signal;
    };

    struct ReturnDescriptor {
        //S2EExecutionState *state;
        uint64_t cr3;
        // TODO: add sourceModuleID and targetModuleID
        FunctionMonitor::ReturnSignal signal;
    };
    typedef std::tr1::unordered_multimap<uint64_t, CallDescriptor> CallDescriptorsMap;
    typedef std::tr1::unordered_multimap<uint64_t, ReturnDescriptor> ReturnDescriptorsMap;

    CallDescriptorsMap m_callDescriptors;
    CallDescriptorsMap m_newCallDescriptors;
    ReturnDescriptorsMap m_returnDescriptors;

    FunctionMonitor *m_plugin;

    /* Get a signal that is emited on function calls. Passing eip = 0 means
       any function, and cr3 = 0 means any cr3 */
    FunctionMonitor::CallSignal* getCallSignal(uint64_t eip, uint64_t cr3 = 0);

    void slotCall(S2EExecutionState *state, uint64_t pc);
    void slotRet(S2EExecutionState *state, uint64_t pc, bool emitSignal);

    void disconnect(const ModuleDescriptor &desc, CallDescriptorsMap &descMap);
    void disconnect(const ModuleDescriptor &desc);

    bool exists(const CallDescriptorsMap &cdm,
                uint64_t eip, uint64_t cr3) const;
public:
    FunctionMonitorState();
    virtual ~FunctionMonitorState();
    virtual FunctionMonitorState* clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    void registerReturnSignal(S2EExecutionState *s, FunctionMonitor::ReturnSignal &sig);

    friend class FunctionMonitor;
};


#define FUNCMON_REGISTER_RETURN(state, fns, func) \
{ \
    FunctionMonitor::ReturnSignal returnSignal; \
    returnSignal.connect(sigc::mem_fun(*this, &func)); \
    fns->registerReturnSignal(state, returnSignal); \
}

#define FUNCMON_REGISTER_RETURN_A(state, fns, func, ...) \
{ \
    FunctionMonitor::ReturnSignal returnSignal; \
    returnSignal.connect(sigc::bind(sigc::mem_fun(*this, &func), __VA_ARGS__)); \
    fns->registerReturnSignal(state, returnSignal); \
}

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_FUNCTIONMONITOR_H
