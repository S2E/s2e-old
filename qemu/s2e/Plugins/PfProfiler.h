#ifndef S2E_PLUGINS_PfProfiler_H
#define S2E_PLUGINS_PfProfiler_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include "ModuleExecutionDetector.h"

#include <stdio.h>

namespace s2e {
namespace plugins {

struct PfProfilerEntry
{
    uint64_t ts, pc, pid;
    uint64_t addr;
    char moduleId[20];
    char isTlbMiss;
    char isWrite;
}__attribute__((packed));

struct PfProfileAggrEntry {
    char moduleId[20];
    uint64_t pid, loadpc;
    uint64_t relativePc; //from the start of the load base
    uint64_t nativePc; //from the start (and including) the native base
    uint64_t count;
};

class PfProfiler : public Plugin
{
    S2E_PLUGIN
public:
    PfProfiler(S2E* s2e): Plugin(s2e) {}

    void initialize();
    bool createTable();
    bool createAggregatedTable();

    bool m_TrackTlbMisses;
    bool m_TrackPageFaults;
private:
    typedef std::pair<uint64_t, uint64_t> PidPcPair;
    typedef std::map<PidPcPair, PfProfileAggrEntry> AggregatedMap;

    std::vector<PfProfilerEntry> m_PfProfilerEntries;

    AggregatedMap m_AggrPageFaults;
    AggregatedMap m_AggrMisses;

    ModuleExecutionDetector *m_execDetector;
    bool m_Aggregated;
    unsigned m_FlushPeriod;

    FILE *m_LogFile;

    void flushTable();
    void flushAggregatedTable();
    void flushAggregatedTable(const AggregatedMap& aggrMap, bool isPfMap);

    void missFaultHandler(S2EExecutionState *state, bool isTlbMiss, uint64_t addr, bool is_write);
    void missFaultHandlerAggregated(S2EExecutionState *state, bool isTlbMiss, uint64_t addr, bool is_write);
    void onTlbMiss(S2EExecutionState *state, uint64_t addr, bool is_write);
    void onPageFault(S2EExecutionState *state, uint64_t addr, bool is_write);
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
