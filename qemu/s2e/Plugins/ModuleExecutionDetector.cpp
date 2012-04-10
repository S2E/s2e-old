/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

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


extern "C" {
#include "config.h"
#include "qemu-common.h"
extern struct CPUX86State *env;
}


#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/s2e_qemu.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/Opcodes.h>

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
        sigc::mem_fun(*this, &ModuleExecutionDetector::moduleLoadListener));

    m_Monitor->onModuleUnload.connect(
        sigc::mem_fun(*this, &ModuleExecutionDetector::moduleUnloadListener));

    m_Monitor->onProcessUnload.connect(
        sigc::mem_fun(*this, &ModuleExecutionDetector::processUnloadListener));

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
        sigc::mem_fun(*this, &ModuleExecutionDetector::onTranslateBlockStart));

    s2e()->getCorePlugin()->onTranslateBlockEnd.connect(
            sigc::mem_fun(*this, &ModuleExecutionDetector::onTranslateBlockEnd));

    s2e()->getCorePlugin()->onException.connect(
        sigc::mem_fun(*this, &ModuleExecutionDetector::exceptionListener));

    s2e()->getCorePlugin()->onCustomInstruction.connect(
        sigc::mem_fun(*this, &ModuleExecutionDetector::onCustomInstruction));

    initializeConfiguration();
}

void ModuleExecutionDetector::initializeConfiguration()
{
    ConfigFile *cfg = s2e()->getConfig();

    ConfigFile::string_list keyList = cfg->getListKeys(getConfigKey());

    if (keyList.size() == 0) {
        s2e()->getWarningsStream() <<  "ModuleExecutionDetector: no configuration keys!" << '\n';
    }

    m_TrackAllModules = cfg->getBool(getConfigKey() + ".trackAllModules");
    m_ConfigureAllModules = cfg->getBool(getConfigKey() + ".configureAllModules");

    foreach2(it, keyList.begin(), keyList.end()) {
        if (*it == "trackAllModules") {
            continue;
        }

        ModuleExecutionCfg d;
        std::stringstream s;
        s << getConfigKey() << "." << *it << ".";
        d.id = *it;

        bool ok = false;
        d.moduleName = cfg->getString(s.str() + "moduleName", "", &ok);
        if (!ok) {
            s2e()->getWarningsStream() << "You must specifiy " << s.str() + "moduleName" << '\n';
            exit(-1);
        }

        d.kernelMode = cfg->getBool(s.str() + "kernelMode", false, &ok);
        if (!ok) {
            s2e()->getWarningsStream() << "You must specifiy " << s.str() + "kernelMode" << '\n';
            exit(-1);
        }


        s2e()->getDebugStream() << "ModuleExecutionDetector: " <<
                "id=" << d.id << " " <<
                "moduleName=" << d.moduleName << " " <<
                "context=" << d.context  << '\n';

        if (m_ConfiguredModulesName.find(d) != m_ConfiguredModulesName.end()) {
            s2e()->getWarningsStream() << "ModuleExecutionDetector: " <<
                    "module names must be unique!" << '\n';
            exit(-1);
        }


        if (m_ConfiguredModulesId.find(d) != m_ConfiguredModulesId.end()) {
            s2e()->getWarningsStream() << "ModuleExecutionDetector: " <<
                    "module ids must be unique!" << '\n';
            exit(-1);
        }

        m_ConfiguredModulesId.insert(d);
        m_ConfiguredModulesName.insert(d);
    }
}
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
bool ModuleExecutionDetector::opAddModuleConfigEntry(S2EExecutionState *state)
{
    bool ok = true;
    //XXX: 32-bits guests only
    uint32_t moduleId, moduleName, isKernelMode;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]), &moduleId, sizeof(moduleId));
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &moduleName, sizeof(moduleName));
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EDX]), &isKernelMode, sizeof(isKernelMode));

    if(!ok) {
        s2e()->getWarningsStream(state)
            << "ModuleExecutionDetector: Could not read parameters.\n";
        return false;
    }

    std::string strModuleId, strModuleName;
    if (!state->readString(moduleId, strModuleId)) {
        s2e()->getWarningsStream(state)
            << "ModuleExecutionDetector: Could not read the module id string.\n";
        return false;
    }

    if (!state->readString(moduleName, strModuleName)) {
        s2e()->getWarningsStream(state)
            << "ModuleExecutionDetector: Could not read the module name string.\n";
        return false;
    }

    ModuleExecutionCfg desc;
    desc.id = strModuleId;
    desc.moduleName = strModuleName;
    desc.kernelMode = (bool) isKernelMode;

    s2e()->getMessagesStream() << "ModuleExecutionDetector: Adding module " <<
            "id=" << desc.id <<
            " moduleName=" << desc.moduleName <<
            " kernelMode=" << desc.kernelMode << "\n";

    if (m_ConfiguredModulesName.find(desc) != m_ConfiguredModulesName.end()) {
        s2e()->getWarningsStream() << "ModuleExecutionDetector: " <<
                "module name " << desc.moduleName << " already exists\n";
        return false;
    }


    if (m_ConfiguredModulesId.find(desc) != m_ConfiguredModulesId.end()) {
        s2e()->getWarningsStream() << "ModuleExecutionDetector: " <<
                "module id " << desc.id << " already exists\n";
        return false;
    }

    m_ConfiguredModulesId.insert(desc);
    m_ConfiguredModulesName.insert(desc);

    return true;
}

void ModuleExecutionDetector::onCustomInstruction(
        S2EExecutionState *state,
        uint64_t operand
        )
{
    if (!OPCODE_CHECK(operand, MODULE_EXECUTION_DETECTOR_OPCODE)) {
        return;
    }

    uint64_t subfunction = OPCODE_GETSUBFUNCTION(operand);

    switch(subfunction) {
        case 0: {
            if (opAddModuleConfigEntry(state)) {
                tb_flush(env);
                state->setPc(state->getPc() + OPCODE_SIZE);
                throw CpuExitException();
            }
            break;
        }
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

    //If module name matches the configured ones, activate.
    s2e()->getDebugStream() << "ModuleExecutionDetector: " <<
            "Module "  << module.Name << " loaded - " <<
            "Base=" <<  hexval(module.LoadBase) << " Size=" << hexval(module.Size);


    ModuleExecutionCfg cfg;
    cfg.moduleName = module.Name;

    if (m_ConfigureAllModules) {
        if (plgState->exists(&module, true)) {
            s2e()->getDebugStream() << " [ALREADY REGISTERED]" << '\n';
        }else {
            s2e()->getDebugStream() << " [REGISTERING]" << '\n';
            plgState->loadDescriptor(module, true);
            onModuleLoad.emit(state, module);
        }
        return;
    }

    ConfiguredModulesByName::iterator it = m_ConfiguredModulesName.find(cfg);
    if (it != m_ConfiguredModulesName.end()) {
        if (plgState->exists(&module, true)) {
            s2e()->getDebugStream() << " [ALREADY REGISTERED ID=" << (*it).id << "]" << '\n';
        }else {
            s2e()->getDebugStream() << " [REGISTERING ID=" << (*it).id << "]" << '\n';
            plgState->loadDescriptor(module, true);
            onModuleLoad.emit(state, module);
        }
        return;
    }

    s2e()->getDebugStream() << '\n';

    if (m_TrackAllModules) {
        if (!plgState->exists(&module, false)) {
            s2e()->getDebugStream() << " [REGISTERING NOT TRACKED]" << '\n';
            plgState->loadDescriptor(module, false);
            onModuleLoad.emit(state, module);
        }
        return;
    }
}

void ModuleExecutionDetector::moduleUnloadListener(
    S2EExecutionState* state, const ModuleDescriptor &module)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    s2e()->getDebugStream() << "Module " << module.Name << " is unloaded" << '\n';

    plgState->unloadDescriptor(module);
}



void ModuleExecutionDetector::processUnloadListener(
    S2EExecutionState* state, uint64_t pid)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    s2e()->getDebugStream() << "Process " << hexval(pid) << " is unloaded\n";

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

const ModuleDescriptor *ModuleExecutionDetector::getModule(S2EExecutionState *state, uint64_t pc, bool tracked)
{
    DECLARE_PLUGINSTATE(ModuleTransitionState, state);
    uint64_t pid = m_Monitor->getPid(state, pc);

    const ModuleDescriptor *currentModule =
            plgState->getDescriptor(pid, pc, tracked);
    return currentModule;
}

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
    //std::cout << "Exception index " << intNb << '\n';
    //onExecution(state, pc);

    DECLARE_PLUGINSTATE(ModuleTransitionState, state);

    //gTRACE("pid=%#"PRIx64" pc=%#"PRIx64"\n", pid, pc);
    if (plgState->m_PreviousModule != NULL) {
        onModuleTransition.emit(state, plgState->m_PreviousModule, NULL);
        plgState->m_PreviousModule = NULL;
    }
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
            s2e_debug_print("Entered module %s\n", currentModule->descriptor.Name.c_str());
        }else {
            s2e_debug_print("Entered unknown module\n");
        }
#endif
        onModuleTransition.emit(state, plgState->m_PreviousModule, currentModule);

        plgState->m_PreviousModule = currentModule;
    }
}

void ModuleExecutionDetector::dumpMemory(S2EExecutionState *state,
                                         llvm::raw_ostream &os_llvm,
                                         uint64_t va, unsigned count)
{
    std::stringstream os;

    uint64_t sp = va;
    for (unsigned i=0; i<count; ++i) {
        klee::ref<klee::Expr> val = state->readMemory(sp + i * sizeof(uint32_t), klee::Expr::Int32);
        if (val.isNull()) {
            continue;
        }

        klee::ConstantExpr *ce = dyn_cast<klee::ConstantExpr>(val);
        if (ce) {
            os << std::hex << "0x" << sp + i * sizeof(uint32_t) << " 0x" << std::setw(sizeof(uint32_t)*2) << std::setfill('0') << val;
            os << std::setfill(' ');

            uint32_t v = ce->getZExtValue(ce->getWidth());
            const ModuleDescriptor *md = getModule(state,  v, false);
            if (md) {
               os << " " << md->Name <<  " 0x" << md->ToNativeBase(v);
               os << " +0x" << md->ToRelative(v);
            }
        }else {
            os << std::hex << "0x" << sp + i * sizeof(uint32_t) << val;
        }

        os << '\n';
    }

    os_llvm << os.str();
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
    foreach2(it, m_Descriptors.begin(), m_Descriptors.end()) {
        delete *it;
    }

    foreach2(it, m_NotTrackedDescriptors.begin(), m_NotTrackedDescriptors.end()) {
        delete *it;
    }
}

ModuleTransitionState* ModuleTransitionState::clone() const
{
    ModuleTransitionState *ret = new ModuleTransitionState();

    foreach2(it, m_Descriptors.begin(), m_Descriptors.end()) {
        ret->m_Descriptors.insert(new ModuleDescriptor(**it));
    }

    foreach2(it, m_NotTrackedDescriptors.begin(), m_NotTrackedDescriptors.end()) {
        assert(*it != m_CachedModule && *it != m_PreviousModule);
        ret->m_NotTrackedDescriptors.insert(new ModuleDescriptor(**it));
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

    p->s2e()->getDebugStream() << "Creating initial module transition state" << '\n';

    return s;
}

const ModuleDescriptor *ModuleTransitionState::getDescriptor(uint64_t pid, uint64_t pc, bool tracked) const
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
    d.Size = 1;
    DescriptorSet::iterator it = m_Descriptors.find(&d);
    if (it != m_Descriptors.end()) {
        m_CachedModule = *it;
        return *it;
    }

    m_CachedModule = NULL;

    if (!tracked) {
        it = m_NotTrackedDescriptors.find(&d);
        if (it != m_NotTrackedDescriptors.end()) {
            //XXX: implement proper caching
            assert(*it != m_CachedModule && *it != m_PreviousModule);
            return *it;
        }
    }

    return NULL;
}

bool ModuleTransitionState::loadDescriptor(const ModuleDescriptor &desc, bool track)
{
    if (track) {
        m_Descriptors.insert(new ModuleDescriptor(desc));
    }else {
        if (m_NotTrackedDescriptors.find(&desc) == m_NotTrackedDescriptors.end()) {
            m_NotTrackedDescriptors.insert(new ModuleDescriptor(desc));
        }
        else {
            return false;
        }
    }
    return true;
}

void ModuleTransitionState::unloadDescriptor(const ModuleDescriptor &desc)
{
    ModuleDescriptor d;
    d.LoadBase = desc.LoadBase;
    d.Pid = desc.Pid;
    d.Size = desc.Size;

    DescriptorSet::iterator it = m_Descriptors.find(&d);
    if (it != m_Descriptors.end()) {
        if (m_CachedModule == *it) {
            m_CachedModule = NULL;
        }

        if (m_PreviousModule == *it) {
            m_PreviousModule = NULL;
        }

        const ModuleDescriptor *md = *it;
        size_t s = m_Descriptors.erase(*it);
        assert(s == 1);
        delete md;
    }

    it = m_NotTrackedDescriptors.find(&d);
    if (it != m_NotTrackedDescriptors.end()) {
        assert(*it != m_CachedModule && *it != m_PreviousModule);
        const ModuleDescriptor *md = *it;
        size_t s = m_NotTrackedDescriptors.erase(*it);
        assert(s == 1);
        delete md;
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

            if (m_CachedModule == *it) {
                m_CachedModule = NULL;
            }

            if (m_PreviousModule == *it) {
                m_PreviousModule = NULL;
            }

            const ModuleDescriptor *md = *it;
            m_Descriptors.erase(*it);
            delete md;

            it = it1;
        }
    }

    //XXX: avoid copy/paste
    for (it = m_NotTrackedDescriptors.begin(); it != m_NotTrackedDescriptors.end(); ) {
        if ((*it)->Pid != pid) {
            ++it;
        }else {
            it1 = it;
            ++it1;

            if (m_CachedModule == *it) {
                m_CachedModule = NULL;
            }

            if (m_PreviousModule == *it) {
                m_PreviousModule = NULL;
            }

            const ModuleDescriptor *md = *it;
            m_NotTrackedDescriptors.erase(*it);
            delete md;

            it = it1;
        }
    }
}

bool ModuleTransitionState::exists(const ModuleDescriptor *desc, bool tracked) const
{
    bool ret;
    ret = m_Descriptors.find(desc) != m_Descriptors.end();
    if (ret) {
        return ret;
    }

    if (tracked) {
        return false;
    }

    return m_NotTrackedDescriptors.find(desc) != m_NotTrackedDescriptors.end();
}
