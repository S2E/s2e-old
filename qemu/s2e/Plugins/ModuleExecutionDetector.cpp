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

    if (keyList.size() == 0) {
        s2e()->getWarningsStream() <<  "ModuleExecutionDetector: no configuration keys!" << std::endl;
    }

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
        s2e()->getDebugStream() << "ModuleExecutionDetector: " <<
                "id=" << d.id << " " <<
                "moduleName=" << d.moduleName << " " <<
                "context=" << d.context  << std::endl;

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
    s2e()->getDebugStream() << "ModuleExecutionDetector: " <<
            "Module " << std::left << std::setw(20) << module.Name << " loaded - " <<
            "Base=0x" << std::hex << module.LoadBase << " Size=0x" << module.Size << std::dec;

    if (m_TrackAllModules) {
        s2e()->getDebugStream() << " [REGISTERING]" << std::endl;
        plgState->loadDescriptor(module);
        return;
    }

    ModuleExecutionCfg cfg;
    cfg.moduleName = module.Name;

    ConfiguredModulesByName::iterator it = m_ConfiguredModulesName.find(cfg);
    if (it != m_ConfiguredModulesName.end()) {
        s2e()->getDebugStream() << " [REGISTERING ID=" << (*it).id << "]" << std::endl;
        plgState->loadDescriptor(module);
        return;
    }

    s2e()->getDebugStream() << std::endl;

}

void ModuleExecutionDetector::moduleUnloadListener(
    S2EExecutionState* state, const ModuleDescriptor &module)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    s2e()->getDebugStream() << "Module " << module.Name << " is unloaded" << std::endl;

    plgState->unloadDescriptor(module);
}



void ModuleExecutionDetector::processUnloadListener(
    S2EExecutionState* state, uint64_t pid)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    s2e()->getDebugStream() << "Process 0x" << std::hex << pid << " is unloaded" << std::dec << std::endl;

    plgState->unloadDescriptorsWithPid(pid);
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

const std::string *ModuleExecutionDetector::getModuleId(const ModuleDescriptor &desc) const
{
    ModuleExecutionCfg cfg;
    cfg.moduleName = desc.Name;

    ConfiguredModulesByName::iterator it = m_ConfiguredModulesName.find(cfg);
    if (it == m_ConfiguredModulesName.end()) {
        return NULL;
    }
    return &(*it).id;
}

void ModuleExecutionDetector::onTranslateBlockStart(
    ExecutionSignal *signal,
    S2EExecutionState *state,
    TranslationBlock *tb,
    uint64_t pc)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    uint64_t pid = m_Monitor->getPid(state, pc);

    const ModuleDescriptor *currentModule =
            plgState->getDescriptor(pid, pc);

    if (currentModule) {
        //S2E::printf(s2e()->getDebugStream(), "Translating block %#"PRIx64" belonging to %s\n",pc, currentModule->Name.c_str());
        signal->connect(sigc::mem_fun(*this,
            &ModuleExecutionDetector::onExecution));

        onModuleTranslateBlockStart.emit(signal, state, *currentModule, tb, pc);
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

    if (currentModule) {
       onModuleTranslateBlockEnd.emit(signal, state, *currentModule, tb, endPc,
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


/**
 *  This returns the descriptor of the module that is currently being executed.
 *  This works only when tracking of all modules is activated.
 */
const ModuleDescriptor *ModuleExecutionDetector::getCurrentDescriptor(S2EExecutionState* state) const
{
    DECLARE_PLUGINSTATE_CONST(ModuleTransitionState, state);

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
        onModuleTransition.emit(state, *plgState->m_PreviousModule, *currentModule);

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
    //assert(false && "Not implemented");
}

ModuleTransitionState* ModuleTransitionState::clone() const
{
    ModuleTransitionState *ret = new ModuleTransitionState();

    foreach2(it, m_Descriptors.begin(), m_Descriptors.end()) {
        ret->m_Descriptors.insert(new ModuleDescriptor(**it));
    }

    if (m_CachedModule) {
        DescriptorSet::iterator it = ret->m_Descriptors.find(m_CachedModule);
        assert(it != ret->m_Descriptors.end());
        ret->m_CachedModule = *it;
    }

    if (m_PreviousModule) {
        DescriptorSet::iterator it = ret->m_Descriptors.find(m_PreviousModule);
        assert(it != ret->m_Descriptors.end());
        ret->m_PreviousModule = *it;
    }

    return ret;
}

PluginState* ModuleTransitionState::factory(Plugin *p, S2EExecutionState *state)
{
    ModuleTransitionState *s = new ModuleTransitionState();

    p->s2e()->getDebugStream() << "Creating initial module transition state" << std::endl;

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
    DescriptorSet::iterator it = m_Descriptors.find(&d);
    if (it != m_Descriptors.end()) {
        m_CachedModule = *it;
        return *it;
    }

    m_CachedModule = NULL;
    return NULL;
}

void ModuleTransitionState::loadDescriptor(const ModuleDescriptor &desc)
{
    m_Descriptors.insert(new ModuleDescriptor(desc));
}

void ModuleTransitionState::unloadDescriptor(const ModuleDescriptor &desc)
{
    ModuleDescriptor d;
    d.LoadBase = desc.LoadBase;
    d.Pid = desc.Pid;
    DescriptorSet::iterator it = m_Descriptors.find(&d);
    if (it != m_Descriptors.end()) {
        m_Descriptors.erase(it);
    }
}

void ModuleTransitionState::unloadDescriptorsWithPid(uint64_t pid)
{
    DescriptorSet::iterator it, it1;

    for (it = m_Descriptors.begin(); it != m_Descriptors.end(); ) {
        if ((*it)->Pid != pid) {
            ++it;
        }else {
            it1 = it;
            ++it1;
            delete *it;
            m_Descriptors.erase(*it);
            it = it1;
        }
    }
}

