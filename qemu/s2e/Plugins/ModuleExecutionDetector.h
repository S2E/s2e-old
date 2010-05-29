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
        return d1.id.compare(d2.id) < 0;
    }
};

struct ModuleExecCfgByName
{
    bool operator()(const ModuleExecutionCfg &d1,
        const ModuleExecutionCfg &d2) const {
        return d1.moduleName.compare(d2.moduleName) < 0;
    }
};

/**
 *  Per-state description of active modules
 */
struct ModuleExecutionDesc {
    std::string id;
    bool kernelMode;
    ModuleDescriptor descriptor;

    bool operator()(const ModuleExecutionDesc &d1,
        const ModuleExecutionDesc &d2) {
            ModuleDescriptor::ModuleByLoadBase cmp;
            return cmp(d1.descriptor, d2.descriptor);
    }

    bool operator==(const ModuleExecutionDesc &d1) {
        return id == d1.id;
    }
};

typedef std::set<ModuleExecutionCfg, ModuleExecCfgById> ConfiguredModulesById;
typedef std::set<ModuleExecutionCfg, ModuleExecCfgByName> ConfiguredModulesByName;

class ModuleExecutionDetector:public Plugin
{
    S2E_PLUGIN

public:
    sigc::signal<
        void, S2EExecutionState *,
        const ModuleExecutionDesc*,
        const ModuleExecutionDesc*> onModuleTransition;

    /** Signal that is emitted on begining and end of code generation
        for each translation block belonging to the module.
    */
    sigc::signal<void, ExecutionSignal*,
            S2EExecutionState*,
            const ModuleExecutionDesc*,
            TranslationBlock*,
            uint64_t /* block PC */>
            onModuleTranslateBlockStart;

    /** Signal that is emitted upon end of translation block of the module */
    sigc::signal<void, ExecutionSignal*,
            S2EExecutionState*,
            const ModuleExecutionDesc*,
            TranslationBlock*,
            uint64_t /* ending instruction pc */,
            bool /* static target is valid */,
            uint64_t /* static target pc */>
            onModuleTranslateBlockEnd;
private:
    OSMonitor *m_Monitor;

    ConfiguredModulesById m_ConfiguredModulesId;
    ConfiguredModulesByName m_ConfiguredModulesName;

    bool m_TrackAllModules;

    void initializeConfiguration();
public:
    ModuleExecutionDetector(S2E* s2e): Plugin(s2e) {}
    virtual ~ModuleExecutionDetector();

    void initialize();

    bool getCurrentModule(
            S2EExecutionState* state,
            ModuleExecutionDesc *desc);


    bool toExecutionDesc(ModuleExecutionDesc *desc, const ModuleDescriptor *md);
    const ModuleDescriptor *getCurrentDescriptor(S2EExecutionState* state);

    const ConfiguredModulesById &getConfiguredModulesById() const {
        return m_ConfiguredModulesId;
    }

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

    bool isModuleConfigured(const std::string &moduleId) const;

    friend class ModuleTransitionState;
};


class ModuleTransitionState:public PluginState
{
private:
    typedef std::set<ModuleDescriptor, ModuleDescriptor::ModuleByLoadBase> DescriptorSet;

    const ModuleDescriptor *m_PreviousModule;
    mutable const ModuleDescriptor *m_CachedModule;

    DescriptorSet m_Descriptors;

#if 0
    void activateModule(const ModuleDescriptor &desc,const ModuleExecutionCfg &cfg);
    void deactivateModule(const ModuleDescriptor &desc);
    void deactivatePid(uint64_t pid);
    const ModuleExecutionDesc *findCurrentModule(uint64_t pid, uint64_t pc) const;
    bool isModuleActive(const std::string &s);
#endif

    const ModuleDescriptor *getDescriptor(uint64_t pid, uint64_t pc) const;
    void loadDescriptor(const ModuleDescriptor *desc);
    void unloadDescriptor(const ModuleDescriptor *desc);
    void unloadDescriptorsWithPid(uint64_t pid);

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
