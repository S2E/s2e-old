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

#ifndef __MODULE_EXECUTION_DETECTOR_H_

#define __MODULE_EXECUTION_DETECTOR_H_

#include <s2e/Plugins/ModuleDescriptor.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>

#include <inttypes.h>
#include "OSMonitor.h"

namespace s2e {
namespace plugins {


/**
 *  Module description from configuration file
 */
struct ModuleExecutionCfg
{
    std::string id;
    std::string moduleName;
    bool kernelMode;
    std::string context;
};

struct ModuleExecCfgById
{
    bool operator()(const ModuleExecutionCfg &d1,
        const ModuleExecutionCfg &d2) const {
        //return d1.compare(d2.id) < 0;
        return d1.id < d2.id;
    }
};

struct ModuleExecCfgByName
{
    bool operator()(const ModuleExecutionCfg &d1,
        const ModuleExecutionCfg &d2) const {
        return d1.moduleName < d2.moduleName;
    }
};

#if 0
struct ModuleExecutionDesc {
    std::string id;
    ModuleDescriptor descriptor;

    bool operator()(const ModuleExecutionDesc &d1,
        const ModuleExecutionDesc &d2) {
            //ModuleDescriptor::ModuleByLoadBase cmp;
            //return cmp(d1.descriptor, d2.descriptor);
            return d1.id < d2.id;
    }

    bool operator==(const ModuleExecutionDesc &d1) {
        return id == d1.id;
    }
};
#endif

typedef std::set<ModuleExecutionCfg, ModuleExecCfgById> ConfiguredModulesById;
typedef std::set<ModuleExecutionCfg, ModuleExecCfgByName> ConfiguredModulesByName;

class ModuleExecutionDetector:public Plugin
{
    S2E_PLUGIN

public:
    sigc::signal<
        void, S2EExecutionState *,
        const ModuleDescriptor *,
        const ModuleDescriptor *> onModuleTransition;

    /** Signal that is emitted on begining and end of code generation
        for each translation block belonging to the module.
    */
    sigc::signal<void, ExecutionSignal*,
            S2EExecutionState*,
            const ModuleDescriptor &,
            TranslationBlock*,
            uint64_t /* block PC */>
            onModuleTranslateBlockStart;

    /** Signal that is emitted upon end of translation block of the module */
    sigc::signal<void, ExecutionSignal*,
            S2EExecutionState*,
            const ModuleDescriptor &,
            TranslationBlock*,
            uint64_t /* ending instruction pc */,
            bool /* static target is valid */,
            uint64_t /* static target pc */>
            onModuleTranslateBlockEnd;

    /** This filters module loads passed by OSInterceptor */
    sigc::signal<void,
       S2EExecutionState*,
       const ModuleDescriptor &
    >onModuleLoad;

private:
    OSMonitor *m_Monitor;

    ConfiguredModulesById m_ConfiguredModulesId;
    ConfiguredModulesByName m_ConfiguredModulesName;

    bool m_TrackAllModules;
    bool m_ConfigureAllModules;

    void initializeConfiguration();
    bool opAddModuleConfigEntry(S2EExecutionState *state);

    void onCustomInstruction(
            S2EExecutionState *state,
            uint64_t operand
            );

    void onTranslateBlockStart(ExecutionSignal *signal,
        S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t pc);

    void onTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc);

    void onExecution(S2EExecutionState *state, uint64_t pc);

    void exceptionListener(
        S2EExecutionState* state,
        unsigned intNb,
        uint64_t pc
    );

    void moduleLoadListener(
        S2EExecutionState* state,
        const ModuleDescriptor &module
    );

    void moduleUnloadListener(
        S2EExecutionState* state,
        const ModuleDescriptor &desc);

    void processUnloadListener(
        S2EExecutionState* state,
        uint64_t pid);

public:
    ModuleExecutionDetector(S2E* s2e): Plugin(s2e) {}
    virtual ~ModuleExecutionDetector();

    void initialize();

    //bool toExecutionDesc(ModuleExecutionDesc *desc, const ModuleDescriptor *md);
    const ModuleDescriptor *getModule(S2EExecutionState *state, uint64_t pc, bool tracked=true);
    const ModuleDescriptor *getCurrentDescriptor(S2EExecutionState* state) const;
    const std::string *getModuleId(const ModuleDescriptor &desc) const;

    void dumpMemory(S2EExecutionState *state,
                    llvm::raw_ostream &os_llvm,
                    uint64_t va, unsigned count);

    const ConfiguredModulesById &getConfiguredModulesById() const {
        return m_ConfiguredModulesId;
    }

    bool isModuleConfigured(const std::string &moduleId) const;

    friend class ModuleTransitionState;
};


class ModuleTransitionState:public PluginState
{
private:
    typedef std::set<const ModuleDescriptor*, ModuleDescriptor::ModuleByLoadBase> DescriptorSet;

    const ModuleDescriptor *m_PreviousModule;
    mutable const ModuleDescriptor *m_CachedModule;

    DescriptorSet m_Descriptors;
    DescriptorSet m_NotTrackedDescriptors;

    const ModuleDescriptor *getDescriptor(uint64_t pid, uint64_t pc, bool tracked=true) const;
    bool loadDescriptor(const ModuleDescriptor &desc, bool track);
    void unloadDescriptor(const ModuleDescriptor &desc);
    void unloadDescriptorsWithPid(uint64_t pid);
    bool exists(const ModuleDescriptor *desc, bool tracked) const;

public:
    sigc::signal<void,
      S2EExecutionState*,
      const ModuleDescriptor*, //PreviousModule
      const ModuleDescriptor*  //NewModule
    >onModuleTransition;

    ModuleTransitionState();
    virtual ~ModuleTransitionState();
    virtual ModuleTransitionState* clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    friend class ModuleExecutionDetector;
};

} // namespace plugins
} // namespace s2e

#endif
