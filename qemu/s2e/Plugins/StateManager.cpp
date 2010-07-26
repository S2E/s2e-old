#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/s2e_qemu.h>

#include "StateManager.h"
#include <klee/Searcher.h>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(StateManager, "Control the deletion/suspension of states", "StateManager",
                  "ModuleExecutionDetector");

void StateManager::initialize()
{
    ConfigFile *cfg = s2e()->getConfig();

    m_timeout = cfg->getInt(getConfigKey() + ".timeout");
    m_timerTicks = 0;

    m_detector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));

    m_detector->onModuleTranslateBlockStart.connect(
            sigc::mem_fun(*this,
                    &StateManager::onNewBlockCovered)
            );

    m_executor = s2e()->getExecutor();

    if (m_timeout > 0) {
        s2e()->getCorePlugin()->onTimer.connect(
                sigc::mem_fun(*this,
                        &StateManager::onTimer)
                );
    }
}

//Reset the timeout every time a new block of the module is translated.
//XXX: this is an approximation. The cache could be flushed in between.
void StateManager::onNewBlockCovered(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t pc)
{
    s2e()->getDebugStream() << "New block " << std::hex << pc << " discovered" << std::endl;
    m_timerTicks = 0;
}

void StateManager::onTimer()
{
    ++m_timerTicks;
    if (m_timerTicks < m_timeout) {
        return;
    }

    s2e()->getDebugStream() << "No more blocks found in " <<
            std::dec << m_timerTicks << " seconds, killing states."
            << std::endl;

    if (!killAllButOneSuccessful()) {
        killAllButCurrent();
    }
}

bool StateManager::succeededState(S2EExecutionState *s)
{
    m_succeeded.insert(s);
    return s2e()->getExecutor()->suspendState(s);
}

bool StateManager::killAllButCurrent()
{
    const std::set<klee::ExecutionState*> &states = s2e()->getExecutor()->getStates();
    std::set<klee::ExecutionState*>::const_iterator it1, it2;

    it1 = states.begin();
    while(it1 != states.end()) {
        if (*it1 == g_s2e_state) {
            ++it1;
            continue;
        }
        it2 = it1;
        ++it2;
        s2e()->getExecutor()->terminateStateOnExit(**it1);
        it1 = it2;
    }
    return true;
}

bool StateManager::killAllButOneSuccessful()
{
    if (m_succeeded.size() < 1) {
        return false;
    }

    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        m_executor->resumeState(*it);
    }

    S2EExecutionState *one =  *m_succeeded.begin();
    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        if (*it == one) {
            continue;
        }else {
            if (*it != g_s2e_state) {
                s2e()->getExecutor()->terminateStateOnExit(**it);
            }
        }
    }

    m_succeeded.clear();

    return true;
}

bool StateManager::empty()
{
    assert(s2e()->getExecutor()->getSearcher());
    return s2e()->getExecutor()->getSearcher()->empty();
}

}
}
