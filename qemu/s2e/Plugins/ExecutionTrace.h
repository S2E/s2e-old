#ifndef S2E__EXECUTION_TRACE_H
#define S2E__EXECUTION_TRACE_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include "ModuleExecutionDetector.h"
#include <fstream>
#include <set>

namespace s2e {
namespace plugins {

struct TraceEntry {
    uint64_t timestamp;
    const ModuleExecutionDesc* desc;
    uint64_t pc, pid;
    unsigned tbType;
};

class ExecutionTrace : public Plugin
{
    S2E_PLUGIN
private:
    std::set<std::string> m_Modules;
    std::vector<TraceEntry> m_Trace;
    ModuleExecutionDetector *m_ExecutionDetector;
    uint64_t m_Ticks;
    uint64_t m_StartTick;
    sigc::connection m_TimerConnection;
public:
    ExecutionTrace(S2E* s2e): Plugin(s2e) {}

    void initialize();
    
private:
    bool createTable();
    bool initSection(const std::string &cfgKey);
    bool initTbTrace(const std::string &cfgKey);
    void flushTable();

    void onTimer();

    void onExecution(S2EExecutionState *state, uint64_t pc,
        const ModuleExecutionDesc*);

    void onTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleExecutionDesc*,
        TranslationBlock *tb,
        uint64_t pc);
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
