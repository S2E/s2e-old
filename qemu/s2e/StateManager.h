#ifndef S2E_STATEMANAGER_H

#define S2E_STATEMANAGER_H

#include "S2E.h"
#include "S2EExecutionState.h"
#include "S2EExecutor.h"
#include <set>

namespace s2e
{

class StateManager
{
public:
    typedef std::set<S2EExecutionState*> StateSet;

private:
    StateSet m_succeeded;
    S2EExecutor *m_executor;

    static StateManager *s_stateManager;

    StateManager(S2E *s2e) {
        m_executor = s2e->getExecutor();
    }

public:

    static StateManager *getManager(S2E *s2e);

    bool succeededState(S2EExecutionState *s);

    bool killAllButOneSuccessful();

    //Checks whether the search has no more states
    bool empty();
};


}

#endif
