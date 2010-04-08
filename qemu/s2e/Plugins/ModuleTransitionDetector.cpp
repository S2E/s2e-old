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

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
        sigc::mem_fun(*this, 
        &ModuleTransitionDetector::slotTranslateBlockStart));
}

void ModuleTransitionDetector::slotTranslateBlockStart(
    ExecutionSignal *signal, uint64_t pc)
{
    signal->connect(sigc::mem_fun(*this, 
        &ModuleTransitionDetector::slotTbExecStart));
}

void ModuleTransitionDetector::slotTbExecStart(
    S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    //Get the module descriptor
    if (plgState->m_PreviousModule) {
        uint64_t prevModStart = plgState->m_PreviousModule->LoadBase;
        uint64_t prevModSize = plgState->m_PreviousModule->Size;
        if (pc >= prevModStart && pc < prevModStart + prevModSize) {
            //We stayed in the same module
            return;
        }
    }

    const ModuleDescriptor *currentModule = NULL;

    if (plgState->m_PreviousModule != currentModule) {
        //onModuleTransition.emit(state, plgState->m_PreviousModule,
          //  currentModule);
        plgState->m_PreviousModule = currentModule;
    }
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
    m_PreviousModule = NULL;
}

ModuleTransitionState::~ModuleTransitionState()
{
    assert(false && "Not implemented");
}

ModuleTransitionState* ModuleTransitionState::clone() const
{
    assert(false && "Not implemented");
    return NULL;
}

PluginState* ModuleTransitionState::factory()
{
    return new ModuleTransitionState();
}