#include <s2e/s2e.h>
#include <s2e/ConfigFile.h>

#include "ModuleTransitionDetector.h"
#include <assert.h>

using namespace s2e;
using namespace plugins;

S2E_DEFINE_PLUGIN(ModuleTransitionDetector, 
                  "Plugin for monitoring module execution", 
                  "ModuleTransitionDetector",
                  "Interceptor");

ModuleTransitionDetector::~ModuleTransitionDetector()
{
    
}

void ModuleTransitionDetector::initialize()
{
    m_Monitor = (OSMonitor*)s2e()->getPlugin("Interceptor");
    assert(m_Monitor);

    m_Monitor->onModuleLoad.connect(
        sigc::mem_fun(*this, 
        &ModuleTransitionDetector::moduleLoadListener));

    m_Monitor->onModuleUnload.connect(
        sigc::mem_fun(*this, 
        &ModuleTransitionDetector::moduleUnloadListener));

    m_Monitor->onProcessUnload.connect(
        sigc::mem_fun(*this, 
        &ModuleTransitionDetector::processUnloadListener));
}

void ModuleTransitionDetector::moduleLoadListener(
    S2EExecutionState* state,
    const ModuleDescriptor &module)
{
    std::cout << "ModuleTransitionDetector - " << module.Name << " loaded" << std::endl;
}

void ModuleTransitionDetector::moduleUnloadListener(
    S2EExecutionState* state, const ModuleDescriptor &desc)
{
    std::cout << "ModuleTransitionDetector - " << desc.Name << " unloaded" << std::endl;
}

void ModuleTransitionDetector::processUnloadListener(
    S2EExecutionState* state, uint64_t pid)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    std::cout << "ModuleTransitionDetector - process " << pid << " killed" << std::endl;

    ModuleDescriptor::MDSet::iterator it, it1;
    
    for(it = plgState->m_LoadedModules.begin(); it != plgState->m_LoadedModules.end(); ) {
        if ((*it).Pid == pid) {
            it1 = it;
            it++;
            plgState->m_LoadedModules.erase(it1);
        }
    }
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

ModuleTransitionState::ModuleTransitionState()
{

}

ModuleTransitionState::~ModuleTransitionState()
{

}

ModuleTransitionState* ModuleTransitionState::clone() const
{
    return NULL;
}

PluginState* ModuleTransitionState::factory()
{
    return new ModuleTransitionState();
}