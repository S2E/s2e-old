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

enum TraceType {
    TRACE_TYPE_TB,
    TRACE_TYPE_INSTR
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
    bool m_StartedTrace;
    bool m_DetectedModule;

    //XXX: this is for all paths.
    //Should redo it properly
    uint64_t m_TotalExecutedInstrCount;
    TraceType m_TraceType;
    
    sigc::connection m_TimerConnection;
public:
    ExecutionTrace(S2E* s2e): Plugin(s2e) {}

    void initialize();
    
private:
    bool createTable();
    bool initSection(const std::string &cfgKey);
    bool initInstrCount(const std::string &cfgKey);
    bool initTbTrace(const std::string &cfgKey);
    void flushTable();
    void flushInstrCount();

    void onTimer();

    void onExecution(S2EExecutionState *state, uint64_t pc,
        const ModuleExecutionDesc*);

    void onTraceInstruction(S2EExecutionState* state, uint64_t pc);

    void onTranslateInstructionStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        TranslationBlock *tb,
        uint64_t pc);


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
