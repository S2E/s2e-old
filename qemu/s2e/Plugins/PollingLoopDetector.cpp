#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>

#include <sstream>

#include "PollingLoopDetector.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(PollingLoopDetector, "Kills states stuck in a polling loop", "PollingLoopDetector",
                  "ModuleExecutionDetector", "Interceptor");

void PollingLoopDetector::initialize()
{
    m_detector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    m_monitor = (OSMonitor*)s2e()->getPlugin("Interceptor");

    m_monitor->onModuleLoad.connect(
            sigc::mem_fun(*this,
                    &PollingLoopDetector::onModuleLoad)
            );

    m_monitor->onModuleUnload.connect(
            sigc::mem_fun(*this,
                    &PollingLoopDetector::onModuleUnload)
            );


}


/**
 *  Read the config section of the module
 */
void PollingLoopDetector::onModuleLoad(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    DECLARE_PLUGINSTATE(PollingLoopDetectorState, state);

    ConfigFile *cfg = s2e()->getConfig();
    const std::string *id = m_detector->getModuleId(module);

    if (!id) {
        s2e()->getDebugStream() << "PollingLoopDetector could not figure out which "
                "module id was passed to onModuleLoad!" << std::endl;
        module.Print(s2e()->getDebugStream());
        return;
    }

    std::stringstream ss;
    ss << getConfigKey() << "." << *id;


    ConfigFile::string_list pollingEntries = cfg->getListKeys(ss.str());

    if (pollingEntries.size() == 0) {
        s2e()->getWarningsStream() << "PollingLoopDetector did not find any configured entry for "
                "the module " << *id <<  "(" << ss.str() << ")" << std::endl;
        return;
    }

    foreach2(it, pollingEntries.begin(), pollingEntries.end()) {
        std::stringstream ss1;
        ss1 << ss.str() << "." << *it;
        ConfigFile::integer_list il = cfg->getIntegerList(ss1.str());
        if (il.size() != 2) {
            s2e()->getWarningsStream() << "PollingLoopDetector entry " << ss1.str() <<
                    " must be of the form {sourcePc, destPc} format" << *id << std::endl;
            continue;
        }

        bool ok = false;
        uint64_t source = cfg->getInt(ss1.str() + "[1]", 0, &ok);
        if (!ok) {
            s2e()->getWarningsStream() << "PollingLoopDetector could not read " << ss1.str() << "[0]" << std::endl;
            continue;
        }

        uint64_t dest = cfg->getInt(ss1.str() + "[2]", 0, &ok);
        if (!ok) {
            s2e()->getWarningsStream() << "PollingLoopDetector could not read " << ss1.str() << "[1]" << std::endl;
            continue;
        }

        //Convert the format to native address
        source = module.ToRuntime(source);
        dest = module.ToRuntime(dest);

        //Insert in the state
        plgState->addEntry(source, dest);
    }

    if (!m_connection.connected()) {
        m_connection = m_detector->onModuleTranslateBlockEnd.connect(
                sigc::mem_fun(*this,
                        &PollingLoopDetector::onModuleTranslateBlockEnd)
                );
    }


}

void PollingLoopDetector::onModuleUnload(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    DECLARE_PLUGINSTATE(PollingLoopDetectorState, state);

    //Remove all the polling entries that belong to the module
    PollingLoopDetectorState::PollingEntries &entries =
            plgState->getEntries();

    PollingLoopDetectorState::PollingEntries::iterator it1, it2;

    it1 = entries.begin();
    while(it1 != entries.end()) {
        const PollingLoopDetectorState::PollingEntry &e = *it1;
        if (module.Contains(e.source)) {
            it2 = it1;
            ++it2;
            entries.erase(it1);
            it1 = it2;
        }else {
            ++it1;
        }
    }
}



/**
 *  Instrument only the blocks where we want to kill the state.
 */
void PollingLoopDetector::onModuleTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    DECLARE_PLUGINSTATE(PollingLoopDetectorState, state);

    //If the target instruction is a kill point, connect the killer.
    if (plgState->isPolling(endPc)) {
        signal->connect(
            sigc::mem_fun(*this, &PollingLoopDetector::onPollingInstruction)
        );
    }
}


void PollingLoopDetector::onPollingInstruction(S2EExecutionState* state, uint64_t sourcePc)
{
    DECLARE_PLUGINSTATE(PollingLoopDetectorState, state);
    if (plgState->isPolling(sourcePc, state->getPc())) {
        std::ostringstream ss;
        ss << "Polling loop from 0x" <<std::hex << sourcePc << " to 0x"
                << state->getPc();

        s2e()->getMessagesStream(state) << ss.str() << std::endl;
        s2e()->getExecutor()->terminateStateEarly(*state, ss.str());
    }
}


///////////////////////////////////////////////////////////////////////////

PollingLoopDetectorState::PollingLoopDetectorState()
{

}

PollingLoopDetectorState::PollingLoopDetectorState(S2EExecutionState *s, Plugin *p)
{

}

PollingLoopDetectorState::~PollingLoopDetectorState()
{

}

PluginState *PollingLoopDetectorState::clone() const
{
    return new PollingLoopDetectorState(*this);
}

PluginState *PollingLoopDetectorState::factory(Plugin *p, S2EExecutionState *s)
{
    return new PollingLoopDetectorState();
}

PollingLoopDetectorState::PollingEntries &PollingLoopDetectorState::getEntries()
{
    return m_pollingEntries;
}

void PollingLoopDetectorState::addEntry(uint64_t source, uint64_t dest)
{
    PollingEntry pe;
    pe.source = source;
    pe.dest = dest;
    m_pollingEntries.insert(pe);
}

bool PollingLoopDetectorState::isPolling(uint64_t source) const
{
    PollingEntry pe;
    pe.source = source;
    return m_pollingEntries.find(pe) != m_pollingEntries.end();
}

bool PollingLoopDetectorState::isPolling(uint64_t source, uint64_t dest) const
{
    PollingEntry pe;
    pe.source = source;
    pe.dest = dest;

    //For now we assume unique source in the list
    PollingEntries::const_iterator it = m_pollingEntries.find(pe);
    if (it != m_pollingEntries.end())  {
        return pe == *it;
    }
    return false;
}

}
}
