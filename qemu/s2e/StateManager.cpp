#include <s2e/Utils.h>

#include "StateManager.h"
#include <klee/Searcher.h>

namespace s2e {

StateManager *StateManager::s_stateManager = NULL;

StateManager *StateManager::getManager(S2E *s2e)
{
    if (!s_stateManager) {
        s_stateManager = new StateManager(s2e);
    }
    return s_stateManager;
}

bool StateManager::succeededState(S2EExecutionState *s)
{
    m_succeeded.insert(s);
    return m_executor->suspendState(s);
}

bool StateManager::killAllButOneSuccessful()
{
    assert(m_succeeded.size() >= 1);

    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        m_executor->resumeState(*it);
    }

    S2EExecutionState *one = NULL;
    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        if (!one) {
            one = *it;
        }else {
            m_executor->terminateStateOnExit(**it);
        }
    }

    return true;
}

bool StateManager::empty()
{
    assert(m_executor->getSearcher());
    return m_executor->getSearcher()->empty();
}

}

