#ifndef S2E_EXECUTIONSTATE_H
#define S2E_EXECUTIONSTATE_H

#include <klee/ExecutionState.h>

// XXX
struct CPUX86State;

//#include <tr1/unordered_map>

namespace s2e {

class Plugin;
class PluginState;

//typedef std::tr1::unordered_map<const Plugin*, PluginState*> PluginStateMap;
typedef std::map<const Plugin*, PluginState*> PluginStateMap;
typedef PluginState* (*PluginStateFactory)();

/** Dummy implementation, just to make events work */
class S2EExecutionState : public klee::ExecutionState
{
protected:
    PluginStateMap m_PluginState;

public:
    CPUX86State* cpuState;
    TranslationBlock *tb;
    uint64_t     cpuPC;

public:
    S2EExecutionState(klee::KFunction *kf)
            : klee::ExecutionState(kf), cpuState(NULL), cpuPC(0) {}

    void selectState(CPUX86State* cpuState, klee::KFunction *kf);
    CPUX86State* getCpuState() const { return cpuState; }
    TranslationBlock *getTb() const { return tb; }

    PluginState* getPluginState(const Plugin *plugin, PluginStateFactory factory) {
        PluginStateMap::iterator it = m_PluginState.find(plugin);
        if (it == m_PluginState.end()) {
            PluginState *ret = factory();
            m_PluginState[plugin] = ret;
            return ret;
        }
        return (*it).second;
    }

    /** Read value from memory concretizing it if necessary */
    uint64_t readMemoryConcrete(uint64_t address, char size);

    /** Write concrete value to memory */
    void writeMemoryConcrete(uint64_t address, uint64_t value, char size);

    uint64_t getPc() const;

    void disableSymbExec();
    void enableSymbExec();
};

}

#endif // S2E_EXECUTIONSTATE_H
