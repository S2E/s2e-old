#ifndef S2E_EXECUTIONSTATE_H
#define S2E_EXECUTIONSTATE_H

#include <klee/ExecutionState.h>

struct CPUX86State;

#include <map>

namespace s2e {

typedef std::map <const Plugin*, PluginState*> PluginStateMap;
typedef PluginState* (*PluginStateFactory)();

/** Dummy implementation, just to make events work */
class S2EExecutionState : public klee::ExecutionState
{
protected:
    CPUX86State* m_cpuState;
    PluginStateMap m_PluginState;

public:
    S2EExecutionState(klee::KFunction *kf)
            : klee::ExecutionState(kf) {}

    void selectState(CPUX86State* cpuState, klee::KFunction *kf);
    CPUX86State* getCpuState() { return m_cpuState; }
    
    PluginState* getPluginState(const Plugin *plugin, PluginStateFactory factory) {
        PluginStateMap::iterator it = m_PluginState.find(plugin);
        if (it == m_PluginState.end()) {
            PluginState *ret = factory();
            m_PluginState[plugin] = ret;
            return ret;
        }
        return (*it).second;
    }
};

}

#endif // S2E_EXECUTIONSTATE_H
