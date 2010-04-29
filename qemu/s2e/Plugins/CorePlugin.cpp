// XXX: qemu stuff should be included before anything from KLEE or LLVM !
extern "C" {
#include <tcg.h>
#include <tcg-op.h>
}

#include "CorePlugin.h"
#include <s2e/S2E.h>
#include <s2e/Utils.h>

#include <s2e/S2EExecutionState.h>
#include <s2e/S2EExecutor.h>

#include <s2e/s2e_qemu.h>

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

    tb->s2e_tb_next[0] = 0;
    tb->s2e_tb_next[1] = 0;
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

void s2e_tcg_execution_handler(
        S2E* s2e, ExecutionSignal* signal, CPUState *env, uint64_t pc)
{
    s2e_update_state_env_pc(g_s2e_state, env, pc);
    signal->emit(g_s2e_state, pc);
}

void s2e_tcg_custom_instr_handler(uint64_t opcode)
{
    g_s2e->getCorePlugin()->onCustomInstruction(g_s2e_state, opcode);
}

void s2e_tcg_emit_custom_instr(S2E* s2e, uint64_t pc, uint64_t val)
{
    TCGv_ptr t0 = tcg_temp_new_i64();
    TCGArg args[1];
    args[0] = GET_TCGV_I64(t0);

#if TCG_TARGET_REG_BITS == 64
    const int sizemask = 16 | 8 | 4 | 2;
    tcg_gen_movi_i64(t0, (tcg_target_ulong) val);
#else
    const int sizemask = 16;
    tcg_gen_movi_i64(t0, (tcg_target_ulong) val);
#endif
    tcg_gen_helperN((void*) s2e_tcg_custom_instr_handler,
                0, sizemask, TCG_CALL_DUMMY_ARG, 1, args);

    tcg_temp_free_i64(t0);
 
}

/* Instrument generated code to emit signal on execution */
void s2e_tcg_instrument_code(S2E* s2e, ExecutionSignal* signal, uint64_t pc)
{
    TCGv_ptr t0 = tcg_temp_new_ptr();
    TCGv_ptr t1 = tcg_temp_new_ptr();
    TCGv_i64 t3 = tcg_temp_new_i64();

    // XXX: here we relay on CPUState being the first tcg global temp
    TCGArg args[4];
    args[0] = GET_TCGV_PTR(t0);
    args[1] = GET_TCGV_PTR(t1);
    args[2] = 0;
    args[3] = GET_TCGV_I64(t3);

#if TCG_TARGET_REG_BITS == 64
    const int sizemask = 16 | 8 | 4 | 2;
    tcg_gen_movi_i64(t0, (tcg_target_ulong) s2e);
    tcg_gen_movi_i64(t1, (tcg_target_ulong) signal);
#else
    const int sizemask = 16;
    tcg_gen_movi_i32(t0, (tcg_target_ulong) s2e);
    tcg_gen_movi_i32(t1, (tcg_target_ulong) signal);
#endif

    tcg_gen_movi_i64(t3, pc);

    tcg_gen_helperN((void*) s2e_tcg_execution_handler,
                0, sizemask, TCG_CALL_DUMMY_ARG, 4, args);

    tcg_temp_free_i64(t3);
    tcg_temp_free_ptr(t1);
    tcg_temp_free_ptr(t0);
}

void s2e_on_translate_block_start(
        S2E* s2e, S2EExecutionState* state, CPUX86State* env,
        TranslationBlock *tb, uint64_t pc)
{
    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    state->cpuState = env;
    s2e->getCorePlugin()->onTranslateBlockStart.emit(signal, state, tb, pc);
    if(!signal->empty()) {
        s2e_tcg_instrument_code(s2e, signal, pc);
        tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
    }
}

void s2e_on_translate_block_end(
        S2E* s2e, S2EExecutionState *state,
        CPUX86State *env, TranslationBlock *tb,
        uint64_t insPc, int staticTarget, uint64_t targetPc)
{
    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    state->cpuState = env;
    s2e->getCorePlugin()->onTranslateBlockEnd.emit(
            signal, state, tb, insPc, 
            staticTarget, targetPc);

    if(!signal->empty()) {
        s2e_tcg_instrument_code(s2e, signal, insPc);
        tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
    }
}

void s2e_on_translate_instruction_start(
        S2E* s2e, S2EExecutionState* state, CPUX86State *env,
        TranslationBlock *tb, uint64_t pc)
{
    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    state->cpuState = env;
    s2e->getCorePlugin()->onTranslateInstructionStart.emit(signal, state, tb, pc);
    if(!signal->empty()) {
        s2e_tcg_instrument_code(s2e, signal, pc);
        tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
    }
}

void s2e_on_translate_instruction_end(
        S2E* s2e, S2EExecutionState* state, CPUX86State *env,
        TranslationBlock *tb, uint64_t pc)
{
    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    state->cpuState = env;
    s2e->getCorePlugin()->onTranslateInstructionEnd.emit(signal, state, tb, pc);
    if(!signal->empty()) {
        s2e_tcg_instrument_code(s2e, signal, pc);
        tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
    }
}

void s2e_on_exception(S2E *s2e, S2EExecutionState* state,
                      CPUX86State *env, unsigned intNb)
{
    state->cpuState = env;
    s2e->getCorePlugin()->onException.emit(state, intNb, state->cpuState->eip);
}
