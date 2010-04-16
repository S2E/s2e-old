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


struct ModuleExecutionDesc
{
    std::string moduleName;
    bool kernelMode;
};

class ModuleExecutionDetector:public Plugin
{
    S2E_PLUGIN

public:
    sigc::signal<
        void, S2EExecutionState *,
        const ModuleDescriptor*, 
        const ModuleDescriptor*> onModuleTransition;

private:
    OSMonitor *m_Monitor;

    std::map<std::string, ModuleExecutionDesc> m_ConfiguredModules;

    void initializeConfiguration();
public:
    ModuleExecutionDetector(S2E* s2e): Plugin(s2e) {}
    virtual ~ModuleExecutionDetector();
    void initialize();

    void onTranslateBlockStart(ExecutionSignal *signal, 
        S2EExecutionState *state,
        uint64_t pc);
    
    void onTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
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

struct ModuleExecStateDesc {
    std::string id;
    std::string imageName;
    bool kernelMode;
    bool isActive;
    ModuleDescriptor descriptor;
};

class ModuleTransitionState:public PluginState
{
private:
    const ModuleDescriptor *m_PreviousModule;

    std::vector<ModuleExecStateDesc> m_ActiveDescriptors;

    void activateModule(const ModuleDescriptor &desc);
    void deactivateModule(const ModuleDescriptor &desc);
    void deactivatePid(uint64_t pid);
    const ModuleDescriptor *findCurrentModule(uint64_t pid, uint64_t pc);

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