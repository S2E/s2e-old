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
        &ModuleExecutionDetector::slotTranslateBlockStart));

    initializeConfiguration();
}

void ModuleExecutionDetector::initializeConfiguration()
{
    ConfigFile *cfg = s2e()->getConfig();

    ConfigFile::string_list keyList = cfg->getKeyList("moduleExecutionDetector");

    foreach2(it, keyList.begin(), keyList.end()) {
        ModuleExecutionDesc d;
        std::stringstream s;
        s << "moduleExecutionDetector." << *it << ".";
        d.moduleName = cfg->getString(s.str() + "moduleName");
        d.kernelMode = cfg->getBool(s.str() + "kernelMode");
        m_ConfiguredModules[*it] = d;
    }
}



void ModuleExecutionDetector::slotTranslateBlockStart(
    ExecutionSignal *signal, uint64_t pc)
{
    //signal->connect(sigc::mem_fun(*this, 
    //    &ModuleExecutionDetector::slotTbExecStart));
}

void ModuleExecutionDetector::slotTbExecStart(
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
            (*it).isActive = false;
        }
    }
}

const ModuleDescriptor *ModuleTransitionState::findCurrentModule(uint64_t pid, uint64_t pc)
{
    return NULL;
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
            (*it).isActive = false;
        }
    }

}