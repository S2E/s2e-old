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
                  "ModuleExecutionDetector", "FunctionMonitor", "Interceptor", "StateManager");

void Annotation::initialize()
{
    m_functionMonitor = static_cast<FunctionMonitor*>(s2e()->getPlugin("FunctionMonitor"));
    m_moduleExecutionDetector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));
    m_osMonitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));
    m_manager = static_cast<StateManager*>(s2e()->getPlugin("StateManager"));

    std::vector<std::string> Sections;
    Sections = s2e()->getConfig()->getListKeys(getConfigKey());
    bool noErrors = true;

    //Reading all sections first
    foreach2(it, Sections.begin(), Sections.end()) {
        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << std::endl;
        std::stringstream sk;
        sk << getConfigKey() << "." << *it;
        if (!initSection(sk.str(), *it)) {
            noErrors = false;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the sections"  <<std::endl;
        exit(-1);
    }

    //Resolving dependencies
    foreach2(it, m_entries.begin(), m_entries.end()) {
        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << (*it)->cfgname << std::endl;
        std::stringstream sk;
        sk << getConfigKey() << "." << (*it)->cfgname;
        if (!resolveDependencies(sk.str(), *it)) {
            noErrors = false;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while resolving dependencies in the sections"  <<std::endl;
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
    std::ostream &os  = s2e()->getWarningsStream();

    e.cfgname = cfgname;

    bool ok;
    e.address = cfg->getInt(entry + ".address", 0, &ok);
    if (!ok) {
        os << "You must specify a valid address for " << entry << ".address!" << std::endl;
        return false;
    }

    e.paramCount = cfg->getInt(entry + ".paramcount", 0, &ok);
    if (!ok) {
        os << "You must specify a valid number of function parameters for " << entry << ".paramcount!" << std::endl;
        return false;
    }

    e.module = cfg->getString(entry + ".module", "", &ok);
    if (!ok) {
        os << "You must specify a valid module for " << entry << ".module!" << std::endl;
        return false;
    }else {
        if (!m_moduleExecutionDetector->isModuleConfigured(e.module)) {
            os << "The module " << e.module << " is not configured in ModuleExecutionDetector!" << std::endl;
            return false;
        }
    }

    e.isActive = cfg->getBool(entry + ".active", false, &ok);
    if (!ok) {
        os << "You must specify whether the entry is active in " << entry << ".active!" << std::endl;
        return false;
    }

    e.executeOnce = cfg->getBool(entry + ".executeonce", false, &ok);
    e.symbolicReturn = cfg->getBool(entry + ".symbolicreturn", false, &ok);
    e.keepReturnPathsCount = cfg->getInt(entry + ".keepReturnPathsCount", 1, &ok);
    e.callAnnotation = cfg->getString(entry + ".callAnnotation", "", &ok);

    ne = new AnnotationCfgEntry(e);
    m_entries.push_back(ne);

    return true;
}

bool Annotation::resolveDependencies(const std::string &entry, AnnotationCfgEntry *e)
{
    ConfigFile *cfg = s2e()->getConfig();
    std::ostream &os  = s2e()->getWarningsStream();

    //Fetch the dependent entries
    bool ok;
    std::vector<std::string> depEntries = cfg->getStringList(entry + ".activateOnEntry", ConfigFile::string_list(), &ok);

    bool found = false;
    foreach2(it, depEntries.begin(), depEntries.end()) {
        //Look for the configuration entrie
        found = false;
        foreach2(eit, m_entries.begin(), m_entries.end()) {
            s2e()->getDebugStream() << "Depentry " << (*eit)->cfgname << std::endl;
            if ((*eit)->cfgname == *it) {
                e->activateOnEntry.push_back(*eit);
                found = true;
            }
        }
        if (!found) {
            os << "Could not find dependency " << *it << std::endl;
            return false;
        }
    }
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

        if (cfg.address - module.NativeBase > module.Size) {
            s2e()->getWarningsStream() << "Specified pc for rule exceeds the size of the loaded module" << std::endl;
        }

        uint64_t funcPc = module.ToRuntime(cfg.address);

        //Register a call monitor for this function
        FunctionMonitor::CallSignal *cs = m_functionMonitor->getCallSignal(state, funcPc, m_osMonitor->getPid(state, funcPc));
        cs->connect(sigc::bind(sigc::mem_fun(*this, &Annotation::onFunctionCall), *it));
    }
}

void Annotation::invokeAnnotation(
        S2EExecutionState* state,
        FunctionMonitorState *fns,
        AnnotationCfgEntry *entry,
        bool isCall
    )
{
    lua_State *L = s2e()->getConfig()->getState();

    S2ELUAExecutionState lua_s2e_state(state);
    LUAAnnotation luaAnnotation(this);

    luaAnnotation.m_isReturn = !isCall;

    lua_getfield(L, LUA_GLOBALSINDEX, entry->callAnnotation.c_str());
    Lunar<S2ELUAExecutionState>::push(L, &lua_s2e_state);
    Lunar<LUAAnnotation>::push(L, &luaAnnotation);
    lua_call(L, 2, 0);

    if (luaAnnotation.m_doKill) {
        std::stringstream ss;
        ss << "Annotation " << entry->cfgname << " killed us" << std::endl;
        s2e()->getExecutor()->terminateStateEarly(*state, "Annotation killed us");
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

void Annotation::onFunctionCall(
        S2EExecutionState* state,
        FunctionMonitorState *fns,
        AnnotationCfgEntry *entry
        )
{
    if (!entry->isActive) {
        return;
    }

    s2e()->getDebugStream() << "Annotation: Invoking call annotation " << entry->cfgname << std::endl;
    invokeAnnotation(state, fns, entry, true);

}

void Annotation::onFunctionRet(
        S2EExecutionState* state,
        AnnotationCfgEntry *entry
        )
{
    s2e()->getDebugStream() << "Annotation: Invoking return annotation "  << entry->cfgname << std::endl;
    invokeAnnotation(state, NULL, entry, false);
}


const char LUAAnnotation::className[] = "LUAAnnotation";

Lunar<LUAAnnotation>::RegType LUAAnnotation::methods[] = {
  LUNAR_DECLARE_METHOD(LUAAnnotation, setSkip),
  LUNAR_DECLARE_METHOD(LUAAnnotation, setKill),
  LUNAR_DECLARE_METHOD(LUAAnnotation, activateRule),
  LUNAR_DECLARE_METHOD(LUAAnnotation, isReturn),
  LUNAR_DECLARE_METHOD(LUAAnnotation, isCall),
  LUNAR_DECLARE_METHOD(LUAAnnotation, succeed),
  {0,0}
};


LUAAnnotation::LUAAnnotation(Annotation *plg)
{
    m_plugin = plg;
    m_doKill = false;
    m_doSkip = false;
    m_isReturn = false;
    m_succeed = false;
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

    g_s2e->getDebugStream() << "LUAAnnotation: setSkip " << m_doSkip << std::endl;
    return 0;
}

int LUAAnnotation::setKill(lua_State *L)
{

    m_doKill = lua_toboolean(L, 1);

    g_s2e->getDebugStream() << "LUAAnnotation: setKill " << m_doSkip << std::endl;
    return 0;
}

int LUAAnnotation::succeed(lua_State *L)
{
    m_succeed = true;
    g_s2e->getDebugStream() << "LUAAnnotation: setKill " << m_doSkip << std::endl;
    return 0;
}





int LUAAnnotation::activateRule(lua_State *L)
{
    std::string rule = luaL_checkstring(L, 1);
    bool activate = lua_toboolean(L, 2);

    g_s2e->getDebugStream() << "LUAAnnotation: setting active state of rule " <<
            rule << " to " << activate << std::endl;

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
    lua_pushboolean(L, m_isReturn);        /* first result */
    return 1;
}

int LUAAnnotation::isCall(lua_State *L)
{
    lua_pushboolean(L, !m_isReturn);        /* first result */
    return 1;
}

} // namespace plugins
} // namespace s2e
