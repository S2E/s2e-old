#include "CorePlugin.h"
#include <s2e/S2E.h>
#include <s2e/Utils.h>

#include <s2e/s2e_qemu.h>

extern "C" {
#include <tcg.h>
#include <tcg-op.h>
}

using namespace std;

namespace s2e {
    S2E_DEFINE_PLUGIN(CorePlugin, "S2E core functionality", "Core",);
} // namespace s2e

using namespace s2e;

/******************************/
/* Functions called from QEMU */

void s2e_tb_alloc(TranslationBlock *tb)
{
    tb->s2e_tb = new S2ETranslationBlock;

    /* Push one copy of a signal to use it as a cache */
    tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
}

void s2e_tb_free(TranslationBlock *tb)
{
    if(tb->s2e_tb) {
        foreach(ExecutionSignal* s, tb->s2e_tb->executionSignals) {
            delete s;
        }
        delete tb->s2e_tb;
    }
}

static S2EExecutionState state;
void s2e_tcg_execution_handler(
                               ExecutionSignal* signal, CPUX86State *env, uint64_t pc)
{
    state.setCpuState(env);
    signal->emit(&state, pc);
}

/* Instrument generated code to emit signal on execution */
void s2e_tcg_instrument_code(ExecutionSignal* signal, uint64_t pc)
{
    TCGv_ptr t0 = tcg_temp_new_ptr();
    TCGv_i64 t1 = tcg_temp_new_i64();

    // XXX: here we relay on CPUState being the first tcg global temp
    TCGArg args[3];
    args[0] = GET_TCGV_PTR(t0);
    args[1] = 0;
    args[2] = GET_TCGV_I64(t1);

#if TCG_TARGET_REG_BITS == 64
    const int sizemask = 8 | 4 | 2;
    tcg_gen_movi_i64(t0, (tcg_target_ulong) signal);
#else
    const int sizemask = 8;
    tcg_gen_movi_i32(t0, (tcg_target_ulong) signal);
#endif

    tcg_gen_movi_i64(t1, pc);

    tcg_gen_helperN((void*) s2e_tcg_execution_handler,
        0, sizemask, TCG_CALL_DUMMY_ARG, 3, args);

    tcg_temp_free_i64(t1);
    tcg_temp_free_ptr(t0);
}

void s2e_on_translate_block_start(S2E* s2e, 
                                  CPUX86State *env,
                                  TranslationBlock *tb, uint64_t pc)
{
    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    state.setCpuState(env);
    s2e->getCorePlugin()->onTranslateBlockStart.emit(signal, &state, pc);
    if(!signal->empty()) {
        s2e_tcg_instrument_code(signal, pc);
        tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
    }
}

void s2e_on_translate_instruction_start(S2E* s2e, 
                                        CPUX86State *env,
                                        TranslationBlock *tb, uint64_t pc)
{
    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    state.setCpuState(env);  
    s2e->getCorePlugin()->onTranslateInstructionStart.emit(signal, &state, pc);
    if(!signal->empty()) {
        s2e_tcg_instrument_code(signal, pc);
        tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
    }
}

void s2e_on_translate_instruction_end(S2E* s2e, 
                                      CPUX86State *env,
                                      TranslationBlock *tb, uint64_t pc)
{
    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    state.setCpuState(env);  
    s2e->getCorePlugin()->onTranslateInstructionEnd.emit(signal, &state, pc);
    if(!signal->empty()) {
        s2e_tcg_instrument_code(signal, pc);
        tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
    }
}
