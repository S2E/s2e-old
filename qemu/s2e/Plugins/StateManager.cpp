#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/s2e_qemu.h>

#include "StateManager.h"
#include <klee/Searcher.h>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(StateManager, "Control the deletion/suspension of states", "StateManager",
                  "ModuleExecutionDetector");

static void sm_callback(S2EExecutionState *s, bool killingState)
{
    StateManager *sm = static_cast<StateManager*>(g_s2e->getPlugin("StateManager"));
    assert(sm);

    if (killingState) {
        assert(s);
        sm->resumeSucceededState(s);
    }

    //Cannot do killAllButCurrent because the current one might be in the process of being killed
    sm->resumeSucceeded();

}

void StateManager::resumeSucceeded()
{
    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        m_executor->resumeState(*it);
    }
    m_succeeded.clear();
}

bool StateManager::resumeSucceededState(S2EExecutionState *s)
{
    if (m_succeeded.find(s) != m_succeeded.end()) {
        m_succeeded.erase(s);
        m_executor->resumeState(s);
        return true;
    }
    return false;

}

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

    s2e()->getExecutor()->setStateManagerCb(sm_callback);
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
        s2e()->getDebugStream() << "There are no successful states to kill..."  << std::endl;
        //killAllButCurrent();
    }
}

bool StateManager::succeedState(S2EExecutionState *s)
{
    s2e()->getDebugStream() << "Succeeding state " << std::dec << s->getID() << std::endl;

    if (m_succeeded.find(s) != m_succeeded.end()) {
        //Avoid suspending states that were consecutively succeeded.
        return false;
    }
    m_succeeded.insert(s);

    return s2e()->getExecutor()->suspendState(s);
}

bool StateManager::killAllButCurrent()
{
    s2e()->getDebugStream() << "Killing all but current " << std::dec << g_s2e_state->getID() << std::endl;
    const std::set<klee::ExecutionState*> &states = s2e()->getExecutor()->getStates();
    std::set<klee::ExecutionState*>::const_iterator it1, it2;

    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        m_executor->resumeState(*it);
    }
    m_succeeded.clear();

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
    if (m_succeeded.size() <= 1) {
        return false;
    }

    s2e()->getDebugStream() << "Killing all but one successful " << std::endl;

    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        m_executor->resumeState(*it);
    }

    S2EExecutionState *one =  *m_succeeded.begin();
    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        if (*it == one) {
            s2e()->getDebugStream() << "Keeping all but one successful " << std::endl;
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
