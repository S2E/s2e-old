#include <s2e/s2e.h>
#include <s2e/s2e_qemu.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include "ModuleExecutionDetector.h"
#include <assert.h>
#include <sstream>

using namespace s2e;
using namespace s2e::plugins;

S2E_DEFINE_PLUGIN(ModuleExecutionDetector, 
                  "Plugin for monitoring module execution", 
                  "ModuleExecutionDetector",
                  "Interceptor");

ModuleExecutionDetector::~ModuleExecutionDetector()
{
    
}

void ModuleExecutionDetector::initialize()
{
    m_Monitor = (OSMonitor*)s2e()->getPlugin("Interceptor");
    assert(m_Monitor);

    m_Monitor->onModuleLoad.connect(
        sigc::mem_fun(*this, 
        &ModuleExecutionDetector::moduleLoadListener));

    m_Monitor->onModuleUnload.connect(
        sigc::mem_fun(*this, 
        &ModuleExecutionDetector::moduleUnloadListener));

    m_Monitor->onProcessUnload.connect(
        sigc::mem_fun(*this, 
        &ModuleExecutionDetector::processUnloadListener));

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
        sigc::mem_fun(*this, 
        &ModuleExecutionDetector::onTranslateBlockStart));

    s2e()->getCorePlugin()->onTranslateBlockEnd.connect(
        sigc::mem_fun(*this, 
        &ModuleExecutionDetector::onTranslateBlockEnd));

    
    s2e()->getCorePlugin()->onException.connect(
        sigc::mem_fun(*this, 
        &ModuleExecutionDetector::exceptionListener));

    
    initializeConfiguration();
}

void ModuleExecutionDetector::initializeConfiguration()
{
    ConfigFile *cfg = s2e()->getConfig();

    ConfigFile::string_list keyList = cfg->getListKeys("moduleExecutionDetector");

    foreach2(it, keyList.begin(), keyList.end()) {
        ModuleExecutionDesc d;
        std::stringstream s;
        s << "moduleExecutionDetector." << *it << ".";
        d.moduleName = cfg->getString(s.str() + "moduleName");
        d.kernelMode = cfg->getBool(s.str() + "kernelMode");
        m_ConfiguredModules[*it] = d;
    }
}


void ModuleExecutionDetector::onTranslateBlockEnd(
        ExecutionSignal *signal,     
        S2EExecutionState* state,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);
    
    uint64_t pid = m_Monitor->getPid(state, endPc);
    
    const ModuleDescriptor *currentModule = 
        plgState->findCurrentModule(pid, endPc);
    
    if (!currentModule) {
        // Outside of any module, do not need
        // to instrument tb exits.
        return;
    }

    if (staticTarget) {
        const ModuleDescriptor *targetModule =
            plgState->findCurrentModule(pid, targetPc);
    
        if (targetModule != currentModule) {
            //Only instrument in case there is a module change
            DPRINTF("Static transition from %#"PRIx64" to %#"PRIx64"\n",
                endPc, targetPc);
            signal->connect(sigc::mem_fun(*this, 
                &ModuleExecutionDetector::onExecution));   
        }
    }else {
        DPRINTF("Dynamic transition from %#"PRIx64" to %#"PRIx64"\n",
                endPc, targetPc);
        //In case of dynamic targets, conservatively
        //instrument code.
        signal->connect(sigc::mem_fun(*this, 
                &ModuleExecutionDetector::onExecution));   

    }

}

void ModuleExecutionDetector::exceptionListener(
                       S2EExecutionState* state,
                       unsigned intNb,
                       uint64_t pc
                       )
{
    //std::cout << "Exception index " << intNb << std::endl;
    onExecution(state, pc);
}

void ModuleExecutionDetector::onTranslateBlockStart(
    ExecutionSignal *signal, 
    S2EExecutionState *state,
    uint64_t pc)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);
    
    uint64_t pid = m_Monitor->getPid(state, pc);
    
    const ModuleDescriptor *currentModule = 
        plgState->findCurrentModule(pid, pc);
    
    if (currentModule) {
        //std::cout << "Translating block belonging to " << currentModule->Name << std::endl;
        signal->connect(sigc::mem_fun(*this, 
            &ModuleExecutionDetector::onExecution));
    }
}

void ModuleExecutionDetector::onExecution(
    S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    uint64_t pid = m_Monitor->getPid(state, pc);
    
    //Get the module descriptor
    if (plgState->m_PreviousModule) {
        uint64_t prevModStart = plgState->m_PreviousModule->LoadBase;
        uint64_t prevModSize = plgState->m_PreviousModule->Size;
        uint64_t prevModPid = plgState->m_PreviousModule->Pid;
        if (pid == prevModPid && pc >= prevModStart && pc < prevModStart + prevModSize) {
            //We stayed in the same module
            return;
        }
    }

    
    const ModuleDescriptor *currentModule = 
        plgState->findCurrentModule(pid, pc);

    if (plgState->m_PreviousModule != currentModule) {
        onModuleTransition.emit(state, plgState->m_PreviousModule,
            currentModule);
        if (currentModule) {
            std::cout << "Entered module " << currentModule->Name << std::endl;
        }else {
            std::cout << "Entered unknown module " << std::endl;
        }
        plgState->m_PreviousModule = currentModule;
    }
}

void ModuleExecutionDetector::moduleLoadListener(
    S2EExecutionState* state,
    const ModuleDescriptor &module)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    std::cout << "ModuleExecutionDetector - " << module.Name << " loaded" << std::endl;
    plgState->activateModule(module);
  
}

void ModuleExecutionDetector::moduleUnloadListener(
    S2EExecutionState* state, const ModuleDescriptor &module)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    std::cout << "ModuleExecutionDetector - " << module.Name << " unloaded" << std::endl;
    plgState->deactivateModule(module);
 
}

void ModuleExecutionDetector::processUnloadListener(
    S2EExecutionState* state, uint64_t pid)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    std::cout << "ModuleExecutionDetector - process " << pid << " killed" << std::endl;
    plgState->deactivatePid(pid);
  
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
    ModuleTransitionState *s = new ModuleTransitionState();
    ModuleExecutionDetector *d = (ModuleExecutionDetector*)g_s2e->getPlugin("ModuleExecutionDetector");

    foreach2(it, d->m_ConfiguredModules.begin(), d->m_ConfiguredModules.end()) {
        ModuleExecStateDesc StateDesc;
        StateDesc.id = (*it).first;
        StateDesc.isActive = false;
        StateDesc.imageName = (*it).second.moduleName;
        StateDesc.kernelMode = (*it).second.kernelMode;
        s->m_ActiveDescriptors.push_back(StateDesc);
    }

    return s;
}

void ModuleTransitionState::activateModule(
    const ModuleDescriptor &desc)
{
    foreach2(it, m_ActiveDescriptors.begin(), m_ActiveDescriptors.end()) {
        if (!(*it).isActive && !(*it).imageName.compare(desc.Name)) {
            if (desc.Pid || (*it).kernelMode) {
                std::cout << "ModuleTransitionState - Module " << desc.Name << " activated" << std::endl;
                (*it).isActive = true;
                (*it).descriptor = desc;
            }
        }
    }
}

void ModuleTransitionState::deactivateModule(
     const ModuleDescriptor &desc)
{
    foreach2(it, m_ActiveDescriptors.begin(), m_ActiveDescriptors.end()) {
        if ((*it).isActive && !(*it).imageName.compare(desc.Name)) {
            std::cout << "ModuleTransitionState - Module " << desc.Name << " deactivated" << std::endl;
            (*it).isActive = false;
        }
    }
}

const ModuleDescriptor *ModuleTransitionState::findCurrentModule(uint64_t pid, uint64_t pc)
{
    foreach2(it, m_ActiveDescriptors.begin(), m_ActiveDescriptors.end()) {
        if (!(*it).isActive) {
            continue;
        }

        if ((*it).descriptor.Pid == pid && (*it).descriptor.Contains(pc)) {
            return &(*it).descriptor;
        }
    }
    return NULL;
}

void ModuleTransitionState::deactivatePid(uint64_t pid)
{
    foreach2(it, m_ActiveDescriptors.begin(), m_ActiveDescriptors.end()) {
        if (!(*it).isActive) {
            continue;
        }

        if ((*it).descriptor.Pid == pid) {
            std::cout << "ModuleTransitionState - Process " << (*it).descriptor.Name << " deactivated" << std::endl;
            (*it).isActive = false;
        }
    }

}
