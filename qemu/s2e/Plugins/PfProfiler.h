#ifndef S2E_PLUGINS_PfProfiler_H
#define S2E_PLUGINS_PfProfiler_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {

struct PfProfilerEntry
{
    uint64_t ts, pc, pid;
    std::string moduleId;
    int isTlbMiss;
    uint64_t addr;
    int isWrite;
};

class PfProfiler : public Plugin
{
    S2E_PLUGIN
public:
    PfProfiler(S2E* s2e): Plugin(s2e) {}

    void initialize();
    bool createTable();

    bool m_TrackTlbMisses;
    bool m_TrackPageFaults;
private:
    void flushTable();
    void missFaultHandler(S2EExecutionState *state, bool isTlbMiss, uint64_t addr, bool is_write);
    void onTlbMiss(S2EExecutionState *state, uint64_t addr, bool is_write);
    void onPageFault(S2EExecutionState *state, uint64_t addr, bool is_write);
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
