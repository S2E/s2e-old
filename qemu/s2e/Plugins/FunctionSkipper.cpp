extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}


#include "FunctionSkipper.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>
#include <sstream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(FunctionSkipper, "Bypasses functions at run-time", "FunctionSkipper",
                  "ModuleExecutionDetector", "FunctionMonitor", "Interceptor");

void FunctionSkipper::initialize()
{
    m_functionMonitor = static_cast<FunctionMonitor*>(s2e()->getPlugin("FunctionMonitor"));
    m_moduleExecutionDetector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));
    m_osMonitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));

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
            &FunctionSkipper::onModuleLoad
        )
    );
}

FunctionSkipper::~FunctionSkipper()
{
    foreach2(it, m_entries.begin(), m_entries.end()) {
        delete *it;
    }
}

bool FunctionSkipper::initSection(const std::string &entry, const std::string &cfgname)
{
    FunctionSkipperCfgEntry e, *ne;

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

    ne = new FunctionSkipperCfgEntry(e);
    m_entries.push_back(ne);

    return true;
}

bool FunctionSkipper::resolveDependencies(const std::string &entry, FunctionSkipperCfgEntry *e)
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
void FunctionSkipper::onModuleLoad(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    foreach2(it, m_entries.begin(), m_entries.end()) {
        const FunctionSkipperCfgEntry &cfg = **it;
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
        cs->connect(sigc::bind(sigc::mem_fun(*this, &FunctionSkipper::onFunctionCall), *it));
    }
}

void FunctionSkipper::onFunctionCall(
        S2EExecutionState* state,
        FunctionMonitorState *fns,
        FunctionSkipperCfgEntry *entry
        )
{
    if (!entry->isActive) {
        return;
    }

    bool skip = !entry->executeOnce || (entry->executeOnce && entry->invocationCount > 0);
    if (skip) {
        if (entry->symbolicReturn) {
            state->undoCallAndJumpToSymbolic();
            klee::ref<klee::Expr> eax = state->createSymbolicValue(klee::Expr::Int32, entry->cfgname + "_ret");
            state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), eax);
        }
        state->bypassFunction(entry->paramCount);
        throw CpuExitException();
    }

    FunctionMonitor::ReturnSignal returnSignal;
    returnSignal.connect(sigc::bind(sigc::mem_fun(*this, &FunctionSkipper::onFunctionRet), entry));
    fns->registerReturnSignal(state, returnSignal);


    ++entry->invocationCount;

}

void FunctionSkipper::onFunctionRet(
        S2EExecutionState* state,
        FunctionSkipperCfgEntry *entry
        )
{
    ++entry->returnCount;
    if (entry->returnCount > entry->keepReturnPathsCount) {
        std::stringstream ss;
        ss << entry->cfgname << " returned " << entry->returnCount << " times" << std::endl;
        s2e()->getExecutor()->terminateStateEarly(*state, ss.str());
        return;
    }
}

} // namespace plugins
} // namespace s2e
