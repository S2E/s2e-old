/**
 *  This plugin tracks the modules which are being executed at any given point.
 *  A module is a piece of code defined by a name. Currently the pieces of code
 *  are derived from the actual executable files reported by the OS monitor.
 *  TODO: allow specifying any kind of regions.
 *
 *  XXX: distinguish between processes and libraries, which should be tracked in all processes.
 *
 *  XXX: might translate a block without instrumentation and reuse it in instrumented part...
 *
 *  NOTE: it is not possible to track relationships between modules here.
 *  For example, tracking of a library of a particular process. Instead, the
 *  plugin tracks all libraries in all processes. This is because the instrumented
 *  code can be shared between different processes. We have to conservatively instrument
 *  all code, otherwise if some interesting code is translated first within the context
 *  of an irrelevant process, there would be no detection instrumentation, and when the
 *  code is executed in the relevant process, the module execution detection would fail.
 */
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
        sigc::mem_fun(
            *this,
            &ModuleExecutionDetector::moduleLoadListener
        )
    );

    m_Monitor->onModuleUnload.connect(
        sigc::mem_fun(
            *this,
            &ModuleExecutionDetector::moduleUnloadListener
        )
    );

    m_Monitor->onProcessUnload.connect(
        sigc::mem_fun(
            *this,
            &ModuleExecutionDetector::processUnloadListener
        )
    );

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
        sigc::mem_fun(
            *this,
            &ModuleExecutionDetector::onTranslateBlockStart
        )
    );

    s2e()->getCorePlugin()->onTranslateBlockEnd.connect(
            sigc::mem_fun(
                *this,
                &ModuleExecutionDetector::onTranslateBlockEnd
            )
        );


    s2e()->getCorePlugin()->onException.connect(
        sigc::mem_fun(
            *this,
            &ModuleExecutionDetector::exceptionListener
        )
    );


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

        if (m_ConfiguredModulesName.find(d) != m_ConfiguredModulesName.end()) {
            s2e()->getWarningsStream() << "ModuleExecutionDetector: " <<
                    "module names must be unique!" << std::endl;
            exit(-1);
        }

        if (m_ConfiguredModulesId.find(d) != m_ConfiguredModulesId.end()) {
            s2e()->getWarningsStream() << "ModuleExecutionDetector: " <<
                    "module ids must be unique!" << std::endl;
            exit(-1);
        }

        m_ConfiguredModulesId.insert(d);
        m_ConfiguredModulesName.insert(d);
    }
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

void ModuleExecutionDetector::moduleLoadListener(
    S2EExecutionState* state,
    const ModuleDescriptor &module)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    //if module name matches the configured ones, then
    //activate.
    DPRINTF("Module %-20s loaded - Base=%#"PRIx64" Size=%#"PRIx64"\n", module.Name.c_str(),
            module.LoadBase, module.Size);

#if 0
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
#endif

    if (m_TrackAllModules) {
        plgState->loadDescriptor(&module);
        return;
    }

    ModuleExecutionCfg cfg;
    cfg.moduleName = module.Name;

    ConfiguredModulesByName::iterator it = m_ConfiguredModulesName.find(cfg);
    if (it != m_ConfiguredModulesName.end()) {
        plgState->loadDescriptor(&module);
    }
}

void ModuleExecutionDetector::moduleUnloadListener(
    S2EExecutionState* state, const ModuleDescriptor &module)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    DPRINTF("Module %s unloaded\n", module.Name.c_str());

#if 0
    std::set<ModuleExecutionCfg, ModuleExecCfgByName>::iterator it;


    plgState->deactivateModule(module);

    if (m_TrackAllModules)
#endif
    {
        plgState->unloadDescriptor(&module);
    }
}



void ModuleExecutionDetector::processUnloadListener(
    S2EExecutionState* state, uint64_t pid)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    DPRINTF("Process %#"PRIx64" unloaded\n", pid);
#if 0
    plgState->deactivatePid(pid);

    if (m_TrackAllModules)
#endif
    {
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


bool ModuleExecutionDetector::toExecutionDesc(
        ModuleExecutionDesc *desc, const ModuleDescriptor *md)
{
    if (!md || !desc) {
        return false;
    }

    ModuleExecutionCfg cfg;
    cfg.moduleName = md->Name;

    ConfiguredModulesByName::iterator it = m_ConfiguredModulesName.find(cfg);
    if (it == m_ConfiguredModulesName.end()) {
        return false;
    }

    desc->id = (*it).id;
    desc->kernelMode = (*it).kernelMode;
    desc->descriptor = *md;
    return true;
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

    //const ModuleExecutionDesc *currentModule =
    //    plgState->findCurrent(pid, pc);
    const ModuleDescriptor *currentModule =
            plgState->getDescriptor(pid, pc);

    if (currentModule) {
        //TRACE("Translating block %#"PRIx64" belonging to %s\n",pc, currentModule->descriptor.Name.c_str());
        signal->connect(sigc::mem_fun(*this,
            &ModuleExecutionDetector::onExecution));

        ModuleExecutionDesc d;
        bool b = toExecutionDesc(&d, currentModule);
        if (b) {
            onModuleTranslateBlockStart.emit(signal, state, &d, tb, pc);
        }
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

    const ModuleDescriptor *currentModule =
            getCurrentDescriptor(state);

    if (!currentModule) {
        // Outside of any module, do not need
        // to instrument tb exits.
        return;
    }


    if (staticTarget) {
        const ModuleDescriptor *targetModule =
            plgState->getDescriptor(m_Monitor->getPid(state, targetPc), targetPc);

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

    ModuleExecutionDesc d;
    bool b = toExecutionDesc(&d, currentModule);
    if (b) {
       onModuleTranslateBlockEnd.emit(signal, state, &d, tb, endPc,
        staticTarget, targetPc);
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

bool ModuleExecutionDetector::getCurrentModule(
        S2EExecutionState* state,
        ModuleExecutionDesc *desc)
{
    //Get the module descriptor
    const ModuleDescriptor *d = getCurrentDescriptor(state);
    if (!d) {
        return false;
    }
    return toExecutionDesc(desc, d);
}

/**
 *  This returns the descriptor of the module that is currently being executed.
 *  This works only when tracking of all modules is activated.
 */
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

    const ModuleDescriptor *currentModule = getCurrentDescriptor(state);

    //gTRACE("pid=%#"PRIx64" pc=%#"PRIx64"\n", pid, pc);
    if (plgState->m_PreviousModule != currentModule) {
#if 0
        if (currentModule) {
            DPRINTF("Entered module %s\n", currentModule->descriptor.Name.c_str());
        }else {
            DPRINTF("Entered unknown module\n");
        }
#endif

        ModuleExecutionDesc cur;
        bool b1 = toExecutionDesc(&cur, currentModule);

        ModuleExecutionDesc prev;
        bool b2 = toExecutionDesc(&prev, plgState->m_PreviousModule);

        if (b1 && b2) {
            onModuleTransition.emit(state, &prev, &cur);
        }else if (b1 && !b2) {
            onModuleTransition.emit(state, &prev, NULL);
        }else if (!b1 && b2) {
            onModuleTransition.emit(state, NULL, &cur);
        }

        plgState->m_PreviousModule = currentModule;
    }
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

ModuleTransitionState::ModuleTransitionState()
{
    m_PreviousModule = NULL;
    m_CachedModule = NULL;
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
    if (m_CachedModule) {
        const ModuleDescriptor &md = *m_CachedModule;
        uint64_t prevModStart = md.LoadBase;
        uint64_t prevModSize = md.Size;
        uint64_t prevModPid = md.Pid;
        if (pid == prevModPid && pc >= prevModStart && pc < prevModStart + prevModSize) {
            //We stayed in the same module
            return m_CachedModule;
        }
    }

    ModuleDescriptor d;
    d.Pid = pid;
    d.LoadBase = pc;
    DescriptorSet::iterator it = m_Descriptors.find(d);
    if (it != m_Descriptors.end()) {
        m_CachedModule = &*it;
        return &*it;
    }

    m_CachedModule = NULL;
    return NULL;
}

void ModuleTransitionState::loadDescriptor(const ModuleDescriptor *desc)
{
    m_Descriptors.insert(*desc);
}

void ModuleTransitionState::unloadDescriptor(const ModuleDescriptor *desc)
{
    m_Descriptors.erase(*desc);
}

void ModuleTransitionState::unloadDescriptorsWithPid(uint64_t pid)
{
    DescriptorSet::iterator it, it1;

    for (it = m_Descriptors.begin(); it != m_Descriptors.end(); ) {
        if ((*it).Pid != pid) {
            ++it;
        }else {
            it1 = it;
            ++it1;
            m_Descriptors.erase(*it);
            it = it1;
        }
    }
}

#if 0
bool ModuleTransitionState::isModuleActive(const std::string &s)
{
    foreach2(it, m_Descriptors.begin(), m_Descriptors.end()) {
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

#endif
