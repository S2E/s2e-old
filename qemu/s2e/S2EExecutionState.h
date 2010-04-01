#ifndef S2E_EXECUTION_STATE_H
#define S2E_EXECUTION_STATE_H

struct CPUX86State;

namespace s2e {

/** Dummy implementation, just to make events work */
class S2EExecutionState
{
protected:
    CPUX86State* m_cpuState;
public:
    S2EExecutionState(CPUX86State *cpuState) : m_cpuState(cpuState) {}

    CPUX86State* getCpuState() { return m_cpuState; }
};

}

#endif // S2E_EXECUTION_STATE_H
