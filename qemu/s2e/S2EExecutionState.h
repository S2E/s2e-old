#ifndef S2E_EXECUTION_STATE_H
#define S2E_EXECUTION_STATE_H

struct CPUX86State;

#include <map>

namespace s2e {

typedef std::map <const Plugin*, PluginState*> PluginStateMap;
typedef PluginState* (*PluginStateFactory)();

/** Dummy implementation, just to make events work */
class S2EExecutionState
{
protected:
    CPUX86State* m_cpuState;
    PluginStateMap m_PluginState;

public:
    S2EExecutionState(CPUX86State *cpuState) : m_cpuState(cpuState) {}
    S2EExecutionState() {}

    CPUX86State* getCpuState() { return m_cpuState; }
    void setCpuState(CPUX86State* state) { m_cpuState = state; }
    
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

#endif // S2E_EXECUTION_STATE_H
