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

#ifndef S2E_PLUGINS_STACKMONITOR_H
#define S2E_PLUGINS_STACKMONITOR_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include "ModuleExecutionDetector.h"
#include "ExecutionStatisticsCollector.h"
#include "OSMonitor.h"

namespace s2e {
namespace plugins {

struct StackFrameInfo {
    uint64_t StackBase;
    uint64_t StackSize;
    uint64_t FrameTop;
    uint64_t FrameSize;
    uint64_t FramePc;
};

typedef std::vector<uint64_t> CallStack;
typedef std::vector<CallStack> CallStacks;

class StackMonitorState;

class StackMonitor : public Plugin
{
    S2E_PLUGIN
public:
    friend class StackMonitorState;
    StackMonitor(S2E* s2e): Plugin(s2e) {}

    void initialize();

    bool getFrameInfo(S2EExecutionState *state, uint64_t sp, bool &onTheStack, StackFrameInfo &info) const;
    void dump(S2EExecutionState *state);

    bool getCallStacks(S2EExecutionState *state, CallStacks &callStacks) const;

    /**
     * Emitted when a new stack frame is setup (e.g., when execution
     * enters a module of interest.
     */
    sigc::signal<void, S2EExecutionState*> onStackCreation;

    /**
     * Emitted when there are no more stack frames anymore.
     */
    sigc::signal<void, S2EExecutionState*> onStackDeletion;

private:
    OSMonitor *m_monitor;
    ModuleExecutionDetector *m_detector;
    ExecutionStatisticsCollector *m_statsCollector;
    bool m_debugMessages;

    sigc::connection m_onTranslateRegisterAccessConnection;

    void onThreadCreate(S2EExecutionState *state, const ThreadDescriptor &thread);
    void onThreadExit(S2EExecutionState *state, const ThreadDescriptor &thread);

    void onModuleTranslateBlockStart(ExecutionSignal *signal,
            S2EExecutionState *state, const ModuleDescriptor &desc,
            TranslationBlock *tb, uint64_t pc);

    void onModuleTranslateBlockEnd(
            ExecutionSignal *signal, S2EExecutionState* state,
            const ModuleDescriptor &desc, TranslationBlock *tb,
            uint64_t endPc, bool staticTarget, uint64_t targetPc);

    void onTranslateRegisterAccess(
            ExecutionSignal *signal, S2EExecutionState* state, TranslationBlock* tb,
            uint64_t pc, uint64_t rmask, uint64_t wmask, bool accessesMemory);

    void onStackPointerModification(S2EExecutionState *state, uint64_t pc, bool isCall);

    void onModuleLoad(S2EExecutionState* state, const ModuleDescriptor &module);
    void onModuleUnload(S2EExecutionState* state, const ModuleDescriptor &module);
    void onModuleTransition(S2EExecutionState* state, const ModuleDescriptor *prev,
                                          const ModuleDescriptor *next);
};


} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_STACKMONITOR_H
