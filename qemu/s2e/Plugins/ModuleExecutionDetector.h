#ifndef __MODULE_EXECUTION_DETECTOR_H_

#define __MODULE_EXECUTION_DETECTOR_H_

#include <s2e/Interceptor/ModuleDescriptor.h>
#include <s2e/Plugins/PluginInterface.h>

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
    std::string moduleName;
    bool kernelMode;
};

/**
 *  Per-state description of active modules
 */
struct ModuleExecutionDesc {
    std::string id;
    std::string imageName;
    bool kernelMode;
    bool isActive;
    ModuleDescriptor descriptor;

    bool operator()(const ModuleExecutionDesc &d1,
        const ModuleExecutionDesc &d2) {
        return d1.id.compare(d2.id) == 0;
    }
};

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

    std::map<std::string, ModuleExecutionCfg> m_ConfiguredModules;

    void initializeConfiguration();
public:
    ModuleExecutionDetector(S2E* s2e): Plugin(s2e) {}
    virtual ~ModuleExecutionDetector();
    void initialize();

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

    friend class ModuleTransitionState;
};


class ModuleTransitionState:public PluginState
{
private:
    const ModuleExecutionDesc *m_PreviousModule;

    std::vector<ModuleExecutionDesc> m_ActiveDescriptors;

    void activateModule(const ModuleDescriptor &desc);
    void deactivateModule(const ModuleDescriptor &desc);
    void deactivatePid(uint64_t pid);
    const ModuleExecutionDesc *findCurrentModule(uint64_t pid, uint64_t pc) const;

public:
    sigc::signal<void, 
      S2EExecutionState*,
      const ModuleDescriptor*, //PreviousModule
      const ModuleDescriptor*  //NewModule
    >onModuleTransition;

    ModuleTransitionState();
    virtual ~ModuleTransitionState();
    virtual ModuleTransitionState* clone() const;
    static PluginState *factory();

    friend class ModuleExecutionDetector;
};

} // namespace plugins
} // namespace s2e

#endif