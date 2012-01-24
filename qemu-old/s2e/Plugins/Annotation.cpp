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

extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}


#include "Annotation.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>
#include <sstream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(Annotation, "Bypasses functions at run-time", "Annotation",
                  "ModuleExecutionDetector", "FunctionMonitor", "Interceptor");

void Annotation::initialize()
{
    m_functionMonitor = static_cast<FunctionMonitor*>(s2e()->getPlugin("FunctionMonitor"));
    m_moduleExecutionDetector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));
    m_osMonitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));
    m_manager = static_cast<StateManager*>(s2e()->getPlugin("StateManager"));

    m_translationEventConnected = false;

    std::vector<std::string> Sections;
    Sections = s2e()->getConfig()->getListKeys(getConfigKey());
    bool noErrors = true;

    //Reading all sections first
    foreach2(it, Sections.begin(), Sections.end()) {
        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << '\n';
        std::stringstream sk;
        sk << getConfigKey() << "." << *it;
        if (!initSection(sk.str(), *it)) {
            noErrors = false;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the sections"  <<'\n';
        exit(-1);
    }

    m_moduleExecutionDetector->onModuleLoad.connect(
        sigc::mem_fun(
            *this,
            &Annotation::onModuleLoad
        )
    );

    Lunar<LUAAnnotation>::Register(s2e()->getConfig()->getState());
}

Annotation::~Annotation()
{
    foreach2(it, m_entries.begin(), m_entries.end()) {
        delete *it;
    }
}

bool Annotation::initSection(const std::string &entry, const std::string &cfgname)
{
    AnnotationCfgEntry e, *ne;

    ConfigFile *cfg = s2e()->getConfig();
    llvm::raw_ostream &os  = s2e()->getWarningsStream();

    e.cfgname = cfgname;

    bool ok;


    e.isActive = cfg->getBool(entry + ".active", false, &ok);
    if (!ok) {
        os << "You must specify whether the entry is active in " << entry << ".active!" << '\n';
        return false;
    }

    e.module = cfg->getString(entry + ".module", "", &ok);
    if (!ok) {
        os << "You must specify a valid module for " << entry << ".module!" << '\n';
        return false;
    }else {
        if (!m_moduleExecutionDetector->isModuleConfigured(e.module)) {
            os << "The module " << e.module << " is not configured in ModuleExecutionDetector!" << '\n';
            return false;
        }
    }


    e.address = cfg->getInt(entry + ".address", 0, &ok);
    if (!ok) {
        os << "You must specify a valid address for " << entry << ".address!" << '\n';
        return false;
    }


    e.beforeInstruction=false;
    e.switchInstructionToSymbolic=false;
    e.annotation = cfg->getString(entry + ".callAnnotation", "", &ok);
    if (!ok || e.annotation=="") {
        e.annotation = cfg->getString(entry + ".instructionAnnotation", "", &ok);
        if (!ok || e.annotation == "") {
            os << "You must specifiy either " << entry << ".callAnnotation or .instructionAnnotation!" << '\n';
            return false;
        }else {
            e.isCallAnnotation = false;
            //Wheter to call the annotation before or after the instruction
            e.beforeInstruction = cfg->getBool(entry + ".beforeInstruction", false, &ok);
            e.switchInstructionToSymbolic = cfg->getBool(entry + ".switchInstructionToSymbolic", false, &ok);
        }
    }else {
        e.isCallAnnotation = true;

        e.paramCount = cfg->getInt(entry + ".paramcount", 0, &ok);
        if (!ok) {
            os << "You must specify a valid number of function parameters for " << entry << ".paramcount!" << '\n';
            return false;
        }
    }

    ne = new AnnotationCfgEntry(e);
    m_entries.insert(ne);

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
//Activate all the relevant rules for each module
void Annotation::onModuleLoad(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    foreach2(it, m_entries.begin(), m_entries.end()) {
        const AnnotationCfgEntry &cfg = **it;
        const std::string *s = m_moduleExecutionDetector->getModuleId(module);
        if (!s || (cfg.module != *s)) {
            continue;
        }

        if (!cfg.isCallAnnotation && !m_translationEventConnected) {
            m_moduleExecutionDetector->onModuleTranslateBlockStart.connect(
                    sigc::mem_fun(*this, &Annotation::onTranslateBlockStart)
                    );

            m_moduleExecutionDetector->onModuleTranslateBlockEnd.connect(
                    sigc::mem_fun(*this, &Annotation::onModuleTranslateBlockEnd)
                    );

            m_translationEventConnected = true;
            continue;
        }

        if (cfg.address - module.NativeBase > module.Size) {
            s2e()->getWarningsStream() << "Specified pc for annotation exceeds the size of the loaded module" << '\n';
        }

        uint64_t funcPc = module.ToRuntime(cfg.address);

        //Register a call monitor for this function
        FunctionMonitor::CallSignal *cs = m_functionMonitor->getCallSignal(state, funcPc, m_osMonitor->getPid(state, funcPc));
        cs->connect(sigc::bind(sigc::mem_fun(*this, &Annotation::onFunctionCall), *it));
    }
}

///////////////////////////////////////////////////////////////////////////////////////
/**
 *  Instrument only the blocks where we want to count the instructions.
 */
void Annotation::onTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t pc)
{
    if (m_tb) {
        m_tbConnectionStart.disconnect();
        m_tbConnectionEnd.disconnect();
    }
    m_tb = tb;

    CorePlugin *plg = s2e()->getCorePlugin();
    m_tbConnectionStart = plg->onTranslateInstructionStart.connect(
            sigc::mem_fun(*this, &Annotation::onTranslateInstructionStart)
    );

    m_tbConnectionEnd = plg->onTranslateInstructionEnd.connect(
            sigc::mem_fun(*this, &Annotation::onTranslateInstructionEnd)
    );
}

void Annotation::onTranslateInstruction(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        TranslationBlock *tb,
        uint64_t pc, bool isStart)
{
    if (tb != m_tb) {
        //We've been suddenly interrupted by some other module
        m_tb = NULL;
        m_tbConnectionStart.disconnect();
        m_tbConnectionEnd.disconnect();
        return;
    }

    //Check that we are in an interesting module
    AnnotationCfgEntry e;
    const ModuleDescriptor *md = m_moduleExecutionDetector->getCurrentDescriptor(state);
    if (!md) {
        return;
    }

    e.isCallAnnotation = false;
    e.module = *m_moduleExecutionDetector->getModuleId(*md);
    e.address = md->ToNativeBase(pc);

    CfgEntries::const_iterator it = m_entries.find(&e);

    if (it == m_entries.end()) {
        return;
    }

    if (isStart && !(*it)->beforeInstruction) {
        return;
    }

    if (!isStart && (*it)->beforeInstruction) {
        return;
    }

    s2e()->getDebugStream() << "Annotation: Instrumenting instruction before=" << isStart <<
   " " << (*it)->beforeInstruction << " with annotation " << (*it)->cfgname << '\n';

    signal->connect(
        sigc::mem_fun(*this, &Annotation::onInstruction)
    );
}

void Annotation::onTranslateInstructionStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        TranslationBlock *tb,
        uint64_t pc)
{
    onTranslateInstruction(signal, state, tb, pc, true);
}


void Annotation::onTranslateInstructionEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        TranslationBlock *tb,
        uint64_t pc)
{
    onTranslateInstruction(signal, state, tb, pc, false);
}

void Annotation::onModuleTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    //TRACE("%"PRIx64" StaticTarget=%d TargetPc=%"PRIx64"\n", endPc, staticTarget, targetPc);

    //Done translating the blocks, no need to instrument anymore.
    if (m_tb) {
        m_tb = NULL;
        m_tbConnectionStart.disconnect();
        m_tbConnectionEnd.disconnect();
    }
}

void Annotation::invokeAnnotation(
        S2EExecutionState* state,
        FunctionMonitorState *fns,
        AnnotationCfgEntry *entry,
        bool isCall, bool isInstruction
    )
{
    lua_State *L = s2e()->getConfig()->getState();

    S2ELUAExecutionState lua_s2e_state(state);
    LUAAnnotation luaAnnotation(this, state);

    luaAnnotation.m_isReturn = !isCall;
    luaAnnotation.m_isInstruction = isInstruction;

    lua_getfield(L, LUA_GLOBALSINDEX, entry->annotation.c_str());
    Lunar<S2ELUAExecutionState>::push(L, &lua_s2e_state);
    Lunar<LUAAnnotation>::push(L, &luaAnnotation);
    lua_call(L, 2, 0);

    if (luaAnnotation.m_doKill) {
        std::stringstream ss;
        ss << "Annotation " << entry->cfgname << " killed us";
        s2e()->getExecutor()->terminateStateEarly(*state, ss.str());
        return;
    }

    if (luaAnnotation.m_doSkip) {
        state->bypassFunction(entry->paramCount);
        throw CpuExitException();
    }

    if (!isCall && luaAnnotation.m_succeed) {
       assert(m_manager && "The StateManager plugin must be active to use succeed() call.");
       m_manager->succeedState(state);
       m_functionMonitor->eraseSp(state, state->getPc());
       throw CpuExitException();
    }

    if (fns) {
        assert(isCall);
        FunctionMonitor::ReturnSignal returnSignal;
        returnSignal.connect(sigc::bind(sigc::mem_fun(*this, &Annotation::onFunctionRet), entry));
        fns->registerReturnSignal(state, returnSignal);
    }
}

void Annotation::onInstruction(S2EExecutionState *state, uint64_t pc)
{
    const ModuleDescriptor *md = m_moduleExecutionDetector->getModule(state, pc, true);
    if (!md) {
        return;
    }

    AnnotationCfgEntry e;
    e.isCallAnnotation = false;
    e.module = *m_moduleExecutionDetector->getModuleId(*md);
    e.address = md->ToNativeBase(pc);

    CfgEntries::iterator it = m_entries.find(&e);

    if (it == m_entries.end()) {
        return;
    }

    if (!(*it)->isActive) {
        return;
    }

    if ((*it)->switchInstructionToSymbolic) {
       state->jumpToSymbolicCpp();
    }


    s2e()->getDebugStream() << "Annotation: Invoking instruction annotation " << (*it)->cfgname <<
            " at " << hexval(e.address) << '\n';
    invokeAnnotation(state, NULL, *it, false, true);

}

void Annotation::onFunctionCall(
        S2EExecutionState* state,
        FunctionMonitorState *fns,
        AnnotationCfgEntry *entry
        )
{
    if (!entry->isActive) {
        return;
    }

    state->undoCallAndJumpToSymbolic();
    s2e()->getDebugStream() << "Annotation: Invoking call annotation " << entry->cfgname << '\n';
    invokeAnnotation(state, fns, entry, true, false);

}

void Annotation::onFunctionRet(
        S2EExecutionState* state,
        AnnotationCfgEntry *entry
        )
{
    state->jumpToSymbolicCpp();
    s2e()->getDebugStream() << "Annotation: Invoking return annotation "  << entry->cfgname << '\n';
    invokeAnnotation(state, NULL, entry, false, false);
}


const char LUAAnnotation::className[] = "LUAAnnotation";

Lunar<LUAAnnotation>::RegType LUAAnnotation::methods[] = {
  LUNAR_DECLARE_METHOD(LUAAnnotation, setSkip),
  LUNAR_DECLARE_METHOD(LUAAnnotation, setKill),
  LUNAR_DECLARE_METHOD(LUAAnnotation, activateRule),
  LUNAR_DECLARE_METHOD(LUAAnnotation, isReturn),
  LUNAR_DECLARE_METHOD(LUAAnnotation, isCall),
  LUNAR_DECLARE_METHOD(LUAAnnotation, succeed),
  LUNAR_DECLARE_METHOD(LUAAnnotation, getValue),
  LUNAR_DECLARE_METHOD(LUAAnnotation, setValue),
  {0,0}
};


LUAAnnotation::LUAAnnotation(Annotation *plg, S2EExecutionState *state)
{
    m_plugin = plg;
    m_doKill = false;
    m_doSkip = false;
    m_isReturn = false;
    m_succeed = false;
    m_isInstruction = false;
    m_state = state;
}

LUAAnnotation::LUAAnnotation(lua_State *lua)
{

}

LUAAnnotation::~LUAAnnotation()
{

}

int LUAAnnotation::setSkip(lua_State *L)
{
    m_doSkip = lua_toboolean(L, 1);

    g_s2e->getDebugStream() << "LUAAnnotation: setSkip " << m_doSkip << '\n';
    return 0;
}

int LUAAnnotation::setKill(lua_State *L)
{

    m_doKill = lua_toboolean(L, 1);

    g_s2e->getDebugStream() << "LUAAnnotation: setKill " << m_doKill << '\n';
    return 0;
}

int LUAAnnotation::succeed(lua_State *L)
{
    m_succeed = true;
    g_s2e->getDebugStream() << "LUAAnnotation: setKill " << m_doSkip << '\n';
    return 0;
}

int LUAAnnotation::activateRule(lua_State *L)
{
    std::string rule = luaL_checkstring(L, 1);
    bool activate = lua_toboolean(L, 2);

    g_s2e->getDebugStream() << "LUAAnnotation: setting active state of rule " <<
            rule << " to " << activate << '\n';

    foreach2(it, m_plugin->m_entries.begin(), m_plugin->m_entries.end()) {
        if ((*it)->cfgname != rule) {
            continue;
        }

        (*it)->isActive = activate != 0;
        lua_pushnumber(L, 1);        /* first result */
        return 1;
    }

    lua_pushnumber(L, 0);        /* first result */
    return 1;
}

int LUAAnnotation::isReturn(lua_State *L)
{
    lua_pushboolean(L, m_isInstruction ? false : m_isReturn);        /* first result */
    return 1;
}

int LUAAnnotation::isCall(lua_State *L)
{
    lua_pushboolean(L, m_isInstruction ? false : !m_isReturn);        /* first result */
    return 1;
}

int LUAAnnotation::setValue(lua_State *L)
{
  std::string key = luaL_checkstring(L, 1);
  uint64_t value = luaL_checknumber(L, 2);

  g_s2e->getDebugStream() << "LUAAnnotation: setValue " << key << "=" << value << '\n';

  DECLARE_PLUGINSTATE_P(m_plugin, AnnotationState, m_state);
  plgState->setValue(key, value);

  return 0;
}

int LUAAnnotation::getValue(lua_State *L)
{
  std::string key = luaL_checkstring(L, 1);

  DECLARE_PLUGINSTATE_P(m_plugin, AnnotationState, m_state);
  uint64_t value = plgState->getValue(key);

  g_s2e->getDebugStream() << "LUAAnnotation: getValue " << key << "=" << value  << '\n';

  lua_pushnumber(L, value);
  return 1;
}

/////////////////////////////

AnnotationState::AnnotationState()
{

}
 
AnnotationState::~AnnotationState()
{

}

AnnotationState* AnnotationState::clone() const
{
    return new AnnotationState(*this);
}

PluginState *AnnotationState::factory(Plugin *p, S2EExecutionState *s)
{
    return new AnnotationState();
}

uint64_t AnnotationState::getValue(const std::string &key)
{
    return m_storage[key];
}

void AnnotationState::setValue(const std::string &key, uint64_t value)
{
    m_storage[key] = value;
}


} // namespace plugins
} // namespace s2e
