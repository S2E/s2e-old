extern "C" {
#include "config.h"
#include "qemu-common.h"
}


#include "CallRetTracer.h"


#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <llvm/System/TimeValue.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(CallRetTracer, "Call/Return tracer plugin",
                  "CallRetTracer"
                  "ExecutionTracer", "ModuleTracer");

CallRetTracer::CallRetTracer(S2E* s2e):EventTracer(s2e)
{

}

CallRetTracer::~CallRetTracer()
{

}

void CallRetTracer::initialize()
{

    m_ModTracer = (ModuleTracer*)s2e()->getPlugin("ModuleTracer");
    assert(m_ModTracer);

    EventTracer::initialize();
    if (!EventTracer::initSections(&CallRetTrancerConfigEntry::factory)) {
        exit(-1);
    }

    if (m_TraceAll) {
        s2e()->getCorePlugin()->onTranslateBlockEnd.connect(
                sigc::mem_fun(*this, &CallRetTracer::onTbEndAll)
            );
    }else {
        m_Detector->onModuleTranslateBlockEnd.connect(
                sigc::mem_fun(*this, &CallRetTracer::onTbEnd));


    }
}

bool CallRetTracer::initSection(TracerConfigEntry *cfgEntry,
                 const std::string &cfgKey, const std::string &entryId)
{
    CallRetTrancerConfigEntry &cfg = (CallRetTrancerConfigEntry&)*cfgEntry;

    cfg.traceCalls = s2e()->getConfig()->getBool(cfgKey + ".traceCalls");
    cfg.traceReturns = s2e()->getConfig()->getBool(cfgKey + ".traceReturns");

    if (!cfg.traceCalls && !cfg.traceReturns) {
        s2e()->getWarningsStream() << "Neither traceCalls nor traceReturns were specified" << std::endl;
        return false;
    }

    return true;
}

void CallRetTracer::onTbEndAll(ExecutionSignal *signal,
                            S2EExecutionState* state,
                            TranslationBlock *tb,
                            uint64_t endPc,
                            bool staticTarget,
                            uint64_t targetPc)
{
    if (tb->s2e_tb_type == TB_CALL || tb->s2e_tb_type == TB_CALL_IND) {
        if (m_Debug) {
            s2e()->getDebugStream() << "CallRetTracer: translating call" << std::endl;
        }

        signal->connect(
                sigc::mem_fun(*this, &CallRetTracer::traceCall)
        );
    }

    if (tb->s2e_tb_type == TB_RET) {
        if (m_Debug) {
            s2e()->getDebugStream() << "CallRetTracer: translating return" << std::endl;
        }

        signal->connect(
                sigc::mem_fun(*this, &CallRetTracer::traceReturn)
        );
    }
}

void CallRetTracer::onTbEnd(ExecutionSignal *signal,
                            S2EExecutionState* state,
                            const ModuleExecutionDesc* desc,
                            TranslationBlock *tb,
                            uint64_t endPc,
                            bool staticTarget,
                            uint64_t targetPc)
{
        EventTracerCfgMap::iterator it = m_Modules.find(desc->id);
        if (it == m_Modules.end()) {
            return;
        }

        CallRetTrancerConfigEntry *trace = (CallRetTrancerConfigEntry*)((*it).second);

        if (tb->s2e_tb_type == TB_CALL && trace->traceCalls) {
            signal->connect(
                    sigc::mem_fun(*this, &CallRetTracer::traceCall)
            );
        }

        if (tb->s2e_tb_type == TB_RET && trace->traceReturns) {
            signal->connect(
                    sigc::mem_fun(*this, &CallRetTracer::traceReturn)
            );
        }


}

void CallRetTracer::traceCall(S2EExecutionState *state, uint64_t pc)
{
    ExecutionTraceCall te;
    te.source = pc;
    te.target = state->getPc();

    m_Tracer->writeData(state, &te, sizeof(te), TRACE_CALL);
}

void CallRetTracer::traceReturn(S2EExecutionState *state, uint64_t pc)
{
    ExecutionTraceCall te;
    te.source = pc;
    te.target = state->getPc();

    m_Tracer->writeData(state, &te, sizeof(te), TRACE_RET);
}

TracerConfigEntry *CallRetTrancerConfigEntry::factory()
{
    return new CallRetTrancerConfigEntry();
}

}
}
