//#define NDEBUG

#include <s2e/S2E.h>
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

    ConfigFile::string_list keyList = cfg->getListKeys(getConfigKey());

    m_TrackAllModules = cfg->getBool(getConfigKey() + ".trackAllModules");

    foreach2(it, keyList.begin(), keyList.end()) {
        if (*it == "trackAllModules") {
            continue;
        }
        
        ModuleExecutionCfg d;
        std::stringstream s;
        s << getConfigKey() << "." << *it << ".";
        d.id = *it;
        d.moduleName = cfg->getString(s.str() + "moduleName");
        d.kernelMode = cfg->getBool(s.str() + "kernelMode");
        TRACE("moduleName=%s kernelMode=%d context=%s\n",
            d.moduleName.c_str(), d.kernelMode, d.context.c_str());
        m_ConfiguredModulesId.insert(d);
        m_ConfiguredModulesName.insert(d);
    }
}


void ModuleExecutionDetector::onTranslateBlockStart(
    ExecutionSignal *signal, 
    S2EExecutionState *state,
    TranslationBlock *tb,
    uint64_t pc)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);
    
    uint64_t pid = m_Monitor->getPid(state, pc);
    
    //XXX: might translate a block without instrumentation
    //and reuse it in instrumented part...
    const ModuleExecutionDesc *currentModule = 
        plgState->findCurrentModule(pid, pc);
    
    if (currentModule) {
        //TRACE("Translating block %#"PRIx64" belonging to %s\n",pc, currentModule->descriptor.Name.c_str());
        signal->connect(sigc::mem_fun(*this, 
            &ModuleExecutionDetector::onExecution));

        onModuleTranslateBlockStart.emit(signal, state, currentModule, tb, pc);
    }
}


void ModuleExecutionDetector::onTranslateBlockEnd(
        ExecutionSignal *signal,     
        S2EExecutionState* state,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);
    
    uint64_t pid = m_Monitor->getPid(state, endPc);
    
    const ModuleExecutionDesc *currentModule = 
        plgState->findCurrentModule(pid, endPc);
    
    if (!currentModule) {
        // Outside of any module, do not need
        // to instrument tb exits.
        return;
    }

  
    if (staticTarget) {
        const ModuleExecutionDesc *targetModule =
            plgState->findCurrentModule(pid, targetPc);
    
        if (targetModule != currentModule) {
            //Only instrument in case there is a module change
            //TRACE("Static transition from %#"PRIx64" to %#"PRIx64"\n",
            //    endPc, targetPc);
            signal->connect(sigc::mem_fun(*this, 
                &ModuleExecutionDetector::onExecution));   
        }
    }else {
        //TRACE("Dynamic transition from %#"PRIx64" to %#"PRIx64"\n",
        //        endPc, targetPc);
        //In case of dynamic targets, conservatively
        //instrument code.
        signal->connect(sigc::mem_fun(*this, 
                &ModuleExecutionDetector::onExecution));   

    }

     onModuleTranslateBlockEnd.emit(signal, state, currentModule, tb, endPc,
        staticTarget, targetPc);


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

const ModuleExecutionDesc *ModuleExecutionDetector::getCurrentModule(S2EExecutionState* state)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);
    //Get the module descriptor
    uint32_t pc = state->getPc();
    uint64_t pid = m_Monitor->getPid(state, state->getPc());
    
    if (plgState->m_PreviousModule) {
        const ModuleDescriptor &md = plgState->m_PreviousModule->descriptor;
        uint64_t prevModStart = md.LoadBase;
        uint64_t prevModSize = md.Size;
        uint64_t prevModPid = md.Pid;
        if (pid == prevModPid && pc >= prevModStart && pc < prevModStart + prevModSize) {
            //We stayed in the same module
            return plgState->m_PreviousModule;
        }
    }

    return plgState->findCurrentModule(pid, pc);
}

const ModuleDescriptor *ModuleExecutionDetector::getCurrentDescriptor(S2EExecutionState* state)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    uint32_t pc = state->getPc();
    uint64_t pid = m_Monitor->getPid(state, state->getPc());
 
    return plgState->getDescriptor(pid, pc);
}

void ModuleExecutionDetector::onExecution(
    S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    //Get the real pc, not the one of the instruction being executed.
    pc = state->getPc();

    uint64_t pid = m_Monitor->getPid(state, state->getPc());
    
    //Get the module descriptor
    if (plgState->m_PreviousModule) {
        const ModuleDescriptor &md = plgState->m_PreviousModule->descriptor;
        uint64_t prevModStart = md.LoadBase;
        uint64_t prevModSize = md.Size;
        uint64_t prevModPid = md.Pid;
        if (pid == prevModPid && pc >= prevModStart && pc < prevModStart + prevModSize) {
            //We stayed in the same module
            return;
        }
    }

    
    const ModuleExecutionDesc *currentModule = 
        plgState->findCurrentModule(pid, pc);

    //gTRACE("pid=%#"PRIx64" pc=%#"PRIx64"\n", pid, pc);
    if (plgState->m_PreviousModule != currentModule) {
#if 0
        if (currentModule) {
            DPRINTF("Entered module %s\n", currentModule->descriptor.Name.c_str());
        }else {
            DPRINTF("Entered unknown module\n");
        }
#endif
        onModuleTransition.emit(state, plgState->m_PreviousModule,
            currentModule);
        
        plgState->m_PreviousModule = currentModule;
    }
}

void ModuleExecutionDetector::moduleLoadListener(
    S2EExecutionState* state,
    const ModuleDescriptor &module)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    //if module name matches the configured ones, then
    //activate.
    DPRINTF("Module %s loaded\n", module.Name.c_str());

    foreach2(it, m_ConfiguredModulesId.begin(), m_ConfiguredModulesId.end()) {
        if ((*it).moduleName != module.Name) {
            continue;
        }
        DPRINTF("Found potential moduleid %s with name %s\n",
            (*it).id.c_str(), (*it).moduleName.c_str());
        if (!plgState->isModuleActive((*it).id)) {
            plgState->activateModule(module, *it);    
            return;
        }
    }

    if (m_TrackAllModules) {
        plgState->loadDescriptor(&module);
    }
  
        
}

void ModuleExecutionDetector::moduleUnloadListener(
    S2EExecutionState* state, const ModuleDescriptor &module)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    DPRINTF("Module %s unloaded\n", module.Name.c_str());
    
    std::set<ModuleExecutionCfg, ModuleExecCfgByName>::iterator it;

    
    plgState->deactivateModule(module);

    if (m_TrackAllModules) {
        plgState->unloadDescriptor(&module);
    }
}



void ModuleExecutionDetector::processUnloadListener(
    S2EExecutionState* state, uint64_t pid)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);
    
    DPRINTF("Process %#"PRIx64" unloaded\n", pid);
    
    plgState->deactivatePid(pid);

    if (m_TrackAllModules) {
        plgState->unloadDescriptorsWithPid(pid);
    }
}


//Check that the module id is valid
bool ModuleExecutionDetector::isModuleConfigured(const std::string &moduleId) const
{
    ModuleExecutionCfg cfg;
    cfg.id = moduleId;

    return m_ConfiguredModulesId.find(cfg) != m_ConfiguredModulesId.end();
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
   
    DPRINTF("Creating initial module transition state\n");

    return s;
}

const ModuleDescriptor *ModuleTransitionState::getDescriptor(uint64_t pid, uint64_t pc) const
{
    ModuleDescriptor d;
    d.Pid = pid;
    d.LoadBase = pc;
    DescriptorSet::iterator it = m_AllDescriptors.find(d);
    if (it != m_AllDescriptors.end()) {
        return &*it;
    }
    return NULL;
}
  
void ModuleTransitionState::loadDescriptor(const ModuleDescriptor *desc)
{
    m_AllDescriptors.insert(*desc);
}
    
void ModuleTransitionState::unloadDescriptor(const ModuleDescriptor *desc)
{
    m_AllDescriptors.erase(*desc);
}

void ModuleTransitionState::unloadDescriptorsWithPid(uint64_t pid)
{
    DescriptorSet::iterator it, it1;

    for (it = m_AllDescriptors.begin(); it != m_AllDescriptors.end(); ) {
        if ((*it).Pid != pid) {
            ++it;
        }else {
            it1 = it;
            ++it1;
            m_AllDescriptors.erase(*it);
            it = it1;
        }
    }
}

bool ModuleTransitionState::isModuleActive(const std::string &s)
{
    foreach2(it, m_ActiveDescriptors.begin(), m_ActiveDescriptors.end()) {
        if ((*it).id == s){
            return true;
        }
    }
    return false;
}

void ModuleTransitionState::activateModule(
    const ModuleDescriptor &desc, const ModuleExecutionCfg &cfg)
{
    ModuleExecutionDesc med;
    DPRINTF("Activating %s/%s (pid=%#"PRIx64")\n", cfg.id.c_str(), desc.Name.c_str(),
        desc.Pid);
    med.id = cfg.id;
    med.kernelMode = cfg.kernelMode;
    med.descriptor = desc;
    
    m_ActiveDescriptors.insert(med);
    
}

void ModuleTransitionState::deactivateModule(
     const ModuleDescriptor &desc)
{
    DPRINTF("Deactivating %s\n", desc.Name.c_str());
    
    ModuleExecutionDesc med;
    med.descriptor = desc;

    int b = m_ActiveDescriptors.erase(med);
    assert(b);
    
}

const ModuleExecutionDesc *ModuleTransitionState::findCurrentModule(uint64_t pid, uint64_t pc) const
{
    foreach2(it, m_ActiveDescriptors.begin(), m_ActiveDescriptors.end()) {
        if ((*it).descriptor.Pid == pid && (*it).descriptor.Contains(pc)) {
            return &(*it);
        }
    }
    return NULL;
}

void ModuleTransitionState::deactivatePid(uint64_t pid)
{
    std::set<ModuleExecutionDesc>::iterator it, it1;

    for(it = m_ActiveDescriptors.begin(); it != m_ActiveDescriptors.end(); )
    {
        if ((*it).descriptor.Pid == pid) {
            DPRINTF("Module %s deactivated\n",  (*it).descriptor.Name.c_str());
            it1 = it;
            ++it1;
            m_ActiveDescriptors.erase(*it);
            it = it1;
        }else {
            ++it;
        }
    }

}


