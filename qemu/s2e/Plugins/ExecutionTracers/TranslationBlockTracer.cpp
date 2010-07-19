extern "C" {
#include "config.h"
//#include "cpu.h"
//#include "exec-all.h"
#include "qemu-common.h"
}


#include "TranslationBlockTracer.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(TranslationBlockTracer, "Tracer for executed translation blocks", "TranslationBlockTracer", "ExecutionTracer",
                  "ModuleExecutionDetector");

void TranslationBlockTracer::initialize()
{
    m_tracer = (ExecutionTracer *)s2e()->getPlugin("ExecutionTracer");
    m_detector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");

    m_detector->onModuleTranslateBlockStart.connect(
            sigc::mem_fun(*this, &TranslationBlockTracer::onModuleTranslateBlockStart)
    );

    m_detector->onModuleTranslateBlockEnd.connect(
            sigc::mem_fun(*this, &TranslationBlockTracer::onModuleTranslateBlockEnd)
    );
}

void TranslationBlockTracer::onModuleTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t pc)
{
    signal->connect(
        sigc::mem_fun(*this, &TranslationBlockTracer::onExecuteBlockStart)
    );
}

void TranslationBlockTracer::onModuleTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    signal->connect(
        sigc::mem_fun(*this, &TranslationBlockTracer::onExecuteBlockEnd)
    );
}

//The real tracing is done here
//-----------------------------

void TranslationBlockTracer::trace(S2EExecutionState *state, uint64_t pc, ExecTraceEntryType type)
{
    ExecutionTraceTb tb;

    tb.pc = pc;
    tb.targetPc = state->getPc();
    tb.tbType = state->getTb()->s2e_tb_type;
    tb.symbMask = 0;

    assert(sizeof(tb.symbMask)*8 >= sizeof(tb.registers)/sizeof(tb.registers[0]));
    for (unsigned i=0; i<sizeof(tb.registers)/sizeof(tb.registers[0]); ++i) {
        //XXX: make it portable across architectures
        unsigned offset = offsetof(CPUX86State, regs) + i*sizeof(tb.registers[0]);
        klee::ref<klee::Expr> r = state->readCpuRegister(offset, klee::Expr::Int32);
        if (klee::ConstantExpr *ce = dyn_cast<klee::ConstantExpr>(r)) {
            tb.registers[i] = (uint32_t)ce->getZExtValue();
        }else {
            tb.symbMask |= 1<<i;
        }
    }

    m_tracer->writeData(state, &tb, sizeof(tb), type);

}

void TranslationBlockTracer::onExecuteBlockStart(S2EExecutionState *state, uint64_t pc)
{
    trace(state, pc, TRACE_TB_START);
}

void TranslationBlockTracer::onExecuteBlockEnd(S2EExecutionState *state, uint64_t pc)
{
    trace(state, pc, TRACE_TB_END);
}

} // namespace plugins
} // namespace s2e
