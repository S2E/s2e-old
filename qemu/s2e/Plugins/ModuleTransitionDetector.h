#ifndef __MODULE_TRANSITION_DETECTOR_H_

#define __MODULE_TRANSITION_DETECTOR_H_

#include <s2e/Interceptor/ModuleDescriptor.h>
#include <s2e/Plugins/PluginInterface.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>

#include <inttypes.h>
#include "OSMonitor.h"

namespace s2e {
namespace plugins {


class ModuleTransitionDetector:public Plugin
{
    S2E_PLUGIN

public:
    sigc::signal<void, const ModuleDescriptor, const ModuleDescriptor> onModuleTransition;

private:
    OSMonitor *m_Monitor;

public:
    ModuleTransitionDetector(S2E* s2e): Plugin(s2e) {}
    virtual ~ModuleTransitionDetector();
    void initialize();

    void slotTranslateBlockStart(ExecutionSignal *signal, uint64_t pc);
    void slotTbExecStart(S2EExecutionState *state, uint64_t pc);

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

};

class ModuleTransitionState:public PluginState
{
private:
    ModuleDescriptor::MDSet m_LoadedModules;
    const ModuleDescriptor *m_PreviousModule;
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

    friend class ModuleTransitionDetector;
};

} // namespace plugins
} // namespace s2e

#endif