#ifndef S2E_PLUGINS_TBTRACER_H
#define S2E_PLUGINS_TBTRACER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include "ExecutionTracer.h"
#include "TraceEntries.h"
#include <s2e/Plugins/ModuleExecutionDetector.h>

namespace s2e {
namespace plugins {

class TranslationBlockTracer : public Plugin
{
    S2E_PLUGIN
public:
    TranslationBlockTracer(S2E* s2e): Plugin(s2e) {}

    void initialize(void);


private:
    ExecutionTracer *m_tracer;
    ModuleExecutionDetector *m_detector;

    void onModuleTranslateBlockStart(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            const ModuleDescriptor &module,
            TranslationBlock *tb,
            uint64_t pc);

    void onModuleTranslateBlockEnd(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            const ModuleDescriptor &module,
            TranslationBlock *tb,
            uint64_t endPc,
            bool staticTarget,
            uint64_t targetPc);

    void onTranslateBlockStart(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            TranslationBlock *tb,
            uint64_t pc);

    void onTranslateBlockEnd(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            TranslationBlock *tb,
            uint64_t endPc,
            bool staticTarget,
            uint64_t targetPc);

    void trace(S2EExecutionState *state, uint64_t pc, ExecTraceEntryType type);

    void onExecuteBlockStart(S2EExecutionState *state, uint64_t pc);
    void onExecuteBlockEnd(S2EExecutionState *state, uint64_t pc);
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
