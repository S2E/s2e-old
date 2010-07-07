#ifndef S2E_PLUGINS_MEMTRACER_H
#define S2E_PLUGINS_MEMTRACER_H

#include <s2e/Plugin.h>
#include <string>
#include "ExecutionTracer.h"

namespace s2e{
namespace plugins{

/** Handler required for KLEE interpreter */
class MemoryTracer : public Plugin
{
    S2E_PLUGIN

private:

public:
    MemoryTracer(S2E* s2e);

    void initialize();
private:
    bool m_monitorPageFaults;
    bool m_monitorTlbMisses;

    bool m_monitorStack;
    uint64_t m_catchAbove;

    uint64_t m_timeTrigger;
    uint64_t m_elapsedTics;
    sigc::connection m_timerConnection;

    ExecutionTracer *m_tracer;

    bool decideTracing(S2EExecutionState *state, uint64_t addr, uint64_t data) const;

    void onDataMemoryAccess(S2EExecutionState *state,
                                   klee::ref<klee::Expr> address,
                                   klee::ref<klee::Expr> hostAddress,
                                   klee::ref<klee::Expr> value,
                                   bool isWrite, bool isIO);

    void onTlbMiss(S2EExecutionState *state, uint64_t addr, bool is_write);
    void onPageFault(S2EExecutionState *state, uint64_t addr, bool is_write);

    void onTimer();

    void enableTracing();

};


}
}

#endif
