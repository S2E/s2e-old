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

#ifndef S2E_PLUGINS_GENDATASEL_H
#define S2E_PLUGINS_GENDATASEL_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include "DataSelector.h"
#include <s2e/Plugins/OSMonitor.h>

namespace s2e {
namespace plugins {

struct RuleCfg
{
    enum Rule {
        RSAKEYGEN, INJECTREG, INJECTMAIN, INJECTMEM
    };

    std::string moduleId;
    Rule rule;
    uint64_t pc;
    unsigned reg;
    bool concrete, size;
    uint32_t value, offset;
    bool makeParamCountSymbolic;
    bool makeParamsSymbolic;
};

struct RuntimeRule
{
    uint64_t pc, pid;
    RuleCfg::Rule rule;
    unsigned reg;
    bool concrete, size;
    uint32_t value, offset;
    bool makeParamCountSymbolic;
    bool makeParamsSymbolic;

    bool operator()(const RuntimeRule &r1, const RuntimeRule &r2) const {
        if (r1.pid == r2.pid) {
            return r1.pc < r2.pc;
        }else {
            return r1.pid < r2.pid;
        }
    }
};

struct RuleCfgByAddr
{
    bool operator()(const RuleCfg &r1, const RuleCfg &r2) const {
        if (r1.moduleId == r2.moduleId) {
            return r1.pc < r2.pc;
        }else {
            return r1.moduleId < r2.moduleId;
        }
    }
};

class GenericDataSelector : public DataSelector
{
    S2E_PLUGIN
public:
    GenericDataSelector(S2E* s2e): DataSelector(s2e) {}

    void initialize();

    typedef std::set<RuleCfg, RuleCfgByAddr> Rules;
private:

    Rules m_Rules;

    OSMonitor *m_Monitor;

    TranslationBlock *m_tb;
    sigc::connection m_tbConnection;

    virtual bool initSection(const std::string &cfgKey, const std::string &svcId);

    void injectRsaGenKey(S2EExecutionState *state);  
    void injectRegister(S2EExecutionState *state, uint64_t pc, unsigned reg, bool concrete, uint32_t val);
    void injectMainArgs(S2EExecutionState *state, const RuntimeRule *rule);

    void injectMemory(S2EExecutionState *state, uint64_t pc,
                                           unsigned reg,
                                           uint32_t offset,
                                           unsigned size,
                                           bool concrete, uint32_t val);

    void onTranslateBlockStart(ExecutionSignal*, S2EExecutionState *state,
                               const ModuleDescriptor &desc,
                               TranslationBlock *tb, uint64_t pc);

    void onTranslateBlockEnd(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            const ModuleDescriptor &desc,
            TranslationBlock *tb,
            uint64_t endPc,
            bool staticTarget,
            uint64_t targetPc);

    void onProcessUnload(S2EExecutionState* state, uint64_t pid);
    void onModuleLoad(S2EExecutionState* state, const ModuleDescriptor &module);
    void onModuleUnload(S2EExecutionState* state, const ModuleDescriptor &module);

    void onTranslateInstructionStart(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            TranslationBlock *tb,
            uint64_t pc);

    void onExecution(S2EExecutionState *state, uint64_t pc);
};

class GenericDataSelectorState: public PluginState
{
private:
    typedef std::set<RuntimeRule, RuntimeRule> RuntimeRules;

    RuntimeRules m_activeRules;

public:


    GenericDataSelectorState();
    GenericDataSelectorState(S2EExecutionState *s, Plugin *p);
    virtual ~GenericDataSelectorState();
    virtual PluginState *clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    bool activateRule(const RuleCfg &rule, const ModuleDescriptor &desc);
    bool deactivateRule(const ModuleDescriptor &desc);
    bool deactivateRule(uint64_t pid);

    bool check(uint64_t pid, uint64_t pc) const;
    const RuntimeRule &getRule(uint64_t pid, uint64_t pc) const;

    friend class GenericDataSelector;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
