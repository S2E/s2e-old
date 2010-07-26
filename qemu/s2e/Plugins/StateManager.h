#ifndef S2E_STATEMANAGER_H

#define S2E_STATEMANAGER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <set>

namespace s2e {
namespace plugins {

class StateManager : public Plugin
{
    S2E_PLUGIN
public:
    StateManager(S2E* s2e): Plugin(s2e) {}
    typedef std::set<S2EExecutionState*> StateSet;

    void initialize();

private:
    StateSet m_succeeded;
    S2EExecutor *m_executor;
    unsigned m_timeout;
    unsigned m_timerTicks;

    ModuleExecutionDetector *m_detector;

    static StateManager *s_stateManager;



    void onTimer();
    void onNewBlockCovered(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            const ModuleDescriptor &module,
            TranslationBlock *tb,
            uint64_t pc);

public:

    bool succeededState(S2EExecutionState *s);

    bool killAllButOneSuccessful();
    bool killAllButCurrent();

    //Checks whether the search has no more states
    bool empty();
};


}
}

#endif
