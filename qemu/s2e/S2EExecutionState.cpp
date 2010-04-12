#include "S2EExecutionState.h"

namespace s2e {

using namespace klee;

void S2EExecutionState::selectState(CPUX86State* cpuState, KFunction *kf)
{
    m_cpuState = cpuState;
}

} // namespace s2e
