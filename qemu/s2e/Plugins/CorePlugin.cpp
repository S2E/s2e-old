/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

// XXX: qemu stuff should be included before anything from KLEE or LLVM !
extern "C" {
#include "tcg-op.h"
#include <qemu-timer.h>

extern struct CPUX86State *env;
}

#include "CorePlugin.h"
#include <s2e/S2E.h>
#include <s2e/Utils.h>

#include <s2e/S2EExecutionState.h>
#include <s2e/S2EExecutor.h>

#include <s2e/s2e_qemu.h>
#include <s2e/s2e_config.h>
#include <s2e/S2ESJLJ.h>


using namespace std;

namespace s2e {
    S2E_DEFINE_PLUGIN(CorePlugin, "S2E core functionality", "Core",);
} // namespace s2e

using namespace s2e;



static void s2e_timer_cb(void *opaque)
{
    CorePlugin *c = (CorePlugin*)opaque;
    g_s2e->getDebugStream() << "Firing timer event" << '\n';

    g_s2e->getExecutor()->updateStats(g_s2e_state);
    c->onTimer.emit();
    qemu_mod_timer(c->getTimer(), qemu_get_clock_ms(rt_clock) + 1000);
}

void CorePlugin::initializeTimers()
{
    s2e()->getDebugStream() << "Initializing periodic timer" << '\n';
    /* Initialize the timer handler */
    m_Timer = qemu_new_timer_ms(rt_clock, s2e_timer_cb, this);
    qemu_mod_timer(m_Timer, qemu_get_clock_ms(rt_clock) + 1000);
}

void CorePlugin::initialize()
{

}

/******************************/
/* Functions called from QEMU */

int g_s2e_enable_signals = true;

void s2e_tcg_execution_handler(void* signal, uint64_t pc)
{
    try {
        ExecutionSignal *s = (ExecutionSignal*)signal;
        if (g_s2e_enable_signals) {
            s->emit(g_s2e_state, pc);
        }
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }
}

void s2e_tcg_custom_instruction_handler(uint64_t arg)
{
    assert(!g_s2e->getCorePlugin()->onCustomInstruction.empty() &&
           "You must activate a plugin that uses custom instructions.");

    try {
        g_s2e->getCorePlugin()->onCustomInstruction.emit(g_s2e_state, arg);
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }
}

void s2e_tcg_emit_custom_instruction(S2E*, uint64_t arg)
{
    TCGv_i64 t0 = tcg_temp_new_i64();
    tcg_gen_movi_i64(t0, arg);

    TCGArg args[1];
    args[0] = GET_TCGV_I64(t0);
    tcg_gen_helperN((void*) s2e_tcg_custom_instruction_handler,
                0, 2, TCG_CALL_DUMMY_ARG, 1, args);

    tcg_temp_free_i64(t0);
}

/* Instrument generated code to emit signal on execution */
/* Next pc, when != -1, indicates with which value to update the program counter
   before calling the annotation. This is useful when instrumenting instructions
   that do not explicitely update the program counter by themselves. */
static void s2e_tcg_instrument_code(S2E*, ExecutionSignal* signal, uint64_t pc, uint64_t nextpc=-1)
{
    TCGv_ptr t0 = tcg_temp_new_ptr();
    TCGv_i64 t1 = tcg_temp_new_i64();

    if (nextpc != (uint64_t)-1) {
        TCGv_i32 tpc = tcg_temp_new_i32();
        TCGv_ptr cpu_env = MAKE_TCGV_PTR(0);
        tcg_gen_movi_i32(tpc, (tcg_target_ulong) nextpc);
        tcg_gen_st_i32(tpc, cpu_env, offsetof(CPUX86State, eip));
        tcg_temp_free_i32(tpc);
    }

    // XXX: here we rely on CPUState being the first tcg global temp
    TCGArg args[2];
    args[0] = GET_TCGV_PTR(t0);
    args[1] = GET_TCGV_I64(t1);

#if TCG_TARGET_REG_BITS == 64
    const int sizemask = 4 | 2;
    tcg_gen_movi_i64(TCGV_PTR_TO_NAT(t0), (tcg_target_ulong) signal);
#else
    const int sizemask = 4;
    tcg_gen_movi_i32(TCGV_PTR_TO_NAT(t0), (tcg_target_ulong) signal);
#endif

    tcg_gen_movi_i64(t1, pc);

    tcg_gen_helperN((void*) s2e_tcg_execution_handler,
                0, sizemask, TCG_CALL_DUMMY_ARG, 2, args);

    tcg_temp_free_i64(t1);
    tcg_temp_free_ptr(t0);
}

void s2e_on_translate_block_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc)
{
    assert(state->isActive());

    ExecutionSignal *signal = static_cast<ExecutionSignal*>(
                                    tb->s2e_tb->executionSignals.back());
    assert(signal->empty());

    try {
        s2e->getCorePlugin()->onTranslateBlockStart.emit(signal, state, tb, pc);
        if(!signal->empty()) {
            s2e_tcg_instrument_code(s2e, signal, pc);
            tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
        }
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }
}

void s2e_on_translate_block_end(
        S2E* s2e, S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t insPc, int staticTarget, uint64_t targetPc)
{
    assert(state->isActive());

    ExecutionSignal *signal = static_cast<ExecutionSignal*>(
                                    tb->s2e_tb->executionSignals.back());
    assert(signal->empty());

    try {
        s2e->getCorePlugin()->onTranslateBlockEnd.emit(
                signal, state, tb, insPc,
                staticTarget, targetPc);
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }

    if(!signal->empty()) {
        s2e_tcg_instrument_code(s2e, signal, insPc);
        tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
    }
}

void s2e_on_translate_instruction_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc)
{
    assert(state->isActive());

    ExecutionSignal *signal = static_cast<ExecutionSignal*>(
                                    tb->s2e_tb->executionSignals.back());
    assert(signal->empty());

    try {
        s2e->getCorePlugin()->onTranslateInstructionStart.emit(signal, state, tb, pc);
        if(!signal->empty()) {
            s2e_tcg_instrument_code(s2e, signal, pc);
            tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
        }
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }
}

void s2e_on_translate_jump_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc, int jump_type)
{
    assert(state->isActive());

    ExecutionSignal *signal = static_cast<ExecutionSignal*>(
                                    tb->s2e_tb->executionSignals.back());
    assert(signal->empty());

    try {
        s2e->getCorePlugin()->onTranslateJumpStart.emit(signal, state, tb,
                                                        pc, jump_type);
        if(!signal->empty()) {
            s2e_tcg_instrument_code(s2e, signal, pc);
            tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
        }
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }
}

//Nextpc is the program counter of the of the instruction that
//follows the one at pc, only if it does not change the control flow.
void s2e_on_translate_instruction_end(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc, uint64_t nextpc)
{
    assert(state->isActive());

    ExecutionSignal *signal = static_cast<ExecutionSignal*>(
                                    tb->s2e_tb->executionSignals.back());
    assert(signal->empty());

    try {
        s2e->getCorePlugin()->onTranslateInstructionEnd.emit(signal, state, tb, pc);
        if(!signal->empty()) {
            s2e_tcg_instrument_code(s2e, signal, pc, nextpc);
            tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
        }
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }
}

void s2e_on_translate_register_access(
        TranslationBlock *tb, uint64_t pc,
        uint64_t readMask, uint64_t writeMask, int isMemoryAccess)
{
    assert(g_s2e_state->isActive());

    ExecutionSignal *signal = static_cast<ExecutionSignal*>(
                                    tb->s2e_tb->executionSignals.back());
    assert(signal->empty());

    try {
        g_s2e->getCorePlugin()->onTranslateRegisterAccessEnd.emit(signal,
                  g_s2e_state, tb, pc, readMask, writeMask, (bool)isMemoryAccess);

        if(!signal->empty()) {
            s2e_tcg_instrument_code(g_s2e, signal, pc);
            tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
        }
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }
}

static void s2e_on_exception_slow(unsigned intNb)
{
    assert(g_s2e_state->isActive());

    try {
        g_s2e->getCorePlugin()->onException.emit(g_s2e_state, intNb, g_s2e_state->getPc());
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }
}

void s2e_on_exception(unsigned intNb)
{
    if(unlikely(!g_s2e->getCorePlugin()->onException.empty())) {
        s2e_on_exception_slow(intNb);
    }
}

void s2e_init_timers(S2E* s2e)
{
    s2e->getCorePlugin()->initializeTimers();
}

static void s2e_trace_memory_access_slow(
        uint64_t vaddr, uint64_t haddr, uint8_t* buf, unsigned size,
        int isWrite, int isIO)
{
    uint64_t value = 0;
    memcpy((void*) &value, buf, size);

    try {
        g_s2e->getCorePlugin()->onDataMemoryAccess.emit(g_s2e_state,
            klee::ConstantExpr::create(vaddr, 64),
            klee::ConstantExpr::create(haddr, 64),
            klee::ConstantExpr::create(value, size*8),
            isWrite, isIO);
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }
}

/**
 * We split the function in two parts so that the common case when
 * there is no instrumentation is as fast as possible.
 */
void s2e_trace_memory_access(
        uint64_t vaddr, uint64_t haddr, uint8_t* buf, unsigned size,
        int isWrite, int isIO)
{
    if(unlikely(!g_s2e->getCorePlugin()->onDataMemoryAccess.empty())) {
        s2e_trace_memory_access_slow(vaddr, haddr, buf, size, isWrite, isIO);
    }
}

void s2e_on_page_fault(S2E *s2e, S2EExecutionState* state, uint64_t addr, int is_write)
{
    try {
        s2e->getCorePlugin()->onPageFault.emit(state, addr, (bool)is_write);
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }
}

void s2e_on_tlb_miss(S2E *s2e, S2EExecutionState* state, uint64_t addr, int is_write)
{
    try {
        s2e->getCorePlugin()->onTlbMiss.emit(state, addr, (bool)is_write);
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(env->jmp_env, 1);
    }
}

void s2e_on_device_registration(S2E *s2e)
{
    s2e->getCorePlugin()->onDeviceRegistration.emit();
}

void s2e_on_device_activation(S2E *s2e, int bus_type, void *bus)
{
    s2e->getCorePlugin()->onDeviceActivation.emit(bus_type, bus);
}


void s2e_trace_port_access(
        S2E *s2e, S2EExecutionState* state,
        uint64_t port, uint64_t value, unsigned size,
        int isWrite)
{
    if(!s2e->getCorePlugin()->onPortAccess.empty()) {
        try {
            s2e->getCorePlugin()->onPortAccess.emit(state,
                klee::ConstantExpr::create(port, 64),
                klee::ConstantExpr::create(value, size),
                isWrite);
        } catch(s2e::CpuExitException&) {
            s2e_longjmp(env->jmp_env, 1);
        }
    }
}

int s2e_is_port_symbolic(struct S2E *s2e, struct S2EExecutionState* state, uint64_t port)
{
    return s2e->getCorePlugin()->isPortSymbolic(port);
}

int s2e_is_mmio_symbolic(uint64_t address, uint64_t size)
{
    return g_s2e->getCorePlugin()->isMmioSymbolic(address, size);
}

int s2e_is_mmio_symbolic_b(uint64_t address)
{
    return g_s2e->getCorePlugin()->isMmioSymbolic(address, 1);
}

int s2e_is_mmio_symbolic_w(uint64_t address)
{
    return g_s2e->getCorePlugin()->isMmioSymbolic(address, 2);
}

int s2e_is_mmio_symbolic_l(uint64_t address)
{
    return g_s2e->getCorePlugin()->isMmioSymbolic(address, 4);
}

int s2e_is_mmio_symbolic_q(uint64_t address)
{
    return g_s2e->getCorePlugin()->isMmioSymbolic(address, 8);
}

void s2e_on_privilege_change(unsigned previous, unsigned current)
{
    assert(g_s2e_state->isActive());

    try {
        g_s2e->getCorePlugin()->onPrivilegeChange.emit(g_s2e_state, previous, current);
    } catch(s2e::CpuExitException&) {
        assert(false && "Cannot throw exceptions here. VM state may be inconsistent at this point.");
    }
}

void s2e_on_page_directory_change(uint64_t previous, uint64_t current)
{
    assert(g_s2e_state->isActive());

    try {
        g_s2e->getCorePlugin()->onPageDirectoryChange.emit(g_s2e_state, previous, current);
    } catch(s2e::CpuExitException&) {
        assert(false && "Cannot throw exceptions here. VM state may be inconsistent at this point.");
    }
}

void s2e_on_initialization_complete(void)
{
    try {
        g_s2e->getCorePlugin()->onInitializationComplete.emit(g_s2e_state);
    } catch(s2e::CpuExitException&) {
        assert(false && "Cannot throw exceptions here. VM state may be inconsistent at this point.");
    }
}
