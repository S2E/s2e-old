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
};

class PfProfiler : public Plugin
{
    S2E_PLUGIN
public:
    PfProfiler(S2E* s2e): Plugin(s2e) {}

    void initialize();
    bool createTable();
private:
    void flushTable();
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
