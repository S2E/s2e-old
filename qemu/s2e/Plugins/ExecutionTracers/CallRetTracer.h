#ifndef S2E_PLUGINS_CALLRETTRACER_H
#define S2E_PLUGINS_CALLRETTRACER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>

#include "EventTracer.h"
#include "ModuleTracer.h"

namespace s2e {
namespace plugins {


struct CallRetTrancerConfigEntry : public TracerConfigEntry
{
    bool traceCalls;
    bool traceReturns;

    CallRetTrancerConfigEntry():TracerConfigEntry() {
        traceCalls = false;
        traceReturns = false;
    }

    virtual ~CallRetTrancerConfigEntry() {

    }

    static TracerConfigEntry *factory();

};

class CallRetTracer : public EventTracer
{
    S2E_PLUGIN

public:
    void initialize();
    CallRetTracer(S2E* s2e);
    virtual ~CallRetTracer();

protected:
    ModuleTracer *m_ModTracer;

    virtual bool initSection(
            TracerConfigEntry *cfgEntry,
            const std::string &cfgKey, const std::string &entryId);

    void onTbEndAll(ExecutionSignal *signal,
                                S2EExecutionState* state,
                                TranslationBlock *tb,
                                uint64_t endPc,
                                bool staticTarget,
                                uint64_t targetPc);


    void onTbEnd(ExecutionSignal *signal,
                                S2EExecutionState* state,
                                const ModuleExecutionDesc* desc,
                                TranslationBlock *tb,
                                uint64_t endPc,
                                bool staticTarget,
                                uint64_t targetPc);

    void traceCall(S2EExecutionState *state, uint64_t pc);
    void traceReturn(S2EExecutionState *state, uint64_t pc);
};

}
}

#endif
