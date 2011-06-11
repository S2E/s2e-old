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

#if 0
    //XXX: debugging code. Will need a generic way of tracing selected portions of pc
    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
            sigc::mem_fun(*this, &TranslationBlockTracer::onTranslateBlockStart)
    );

    s2e()->getCorePlugin()->onTranslateBlockEnd.connect(
            sigc::mem_fun(*this, &TranslationBlockTracer::onTranslateBlockEnd)
    );
#endif
}

void TranslationBlockTracer::onTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        TranslationBlock *tb,
        uint64_t pc)
{
    uint64_t rlow = 0x4746c4 - 0x400000 + 0x804d7000;
    uint64_t rhigh = 0x474e23 - 0x400000 + 0x804d7000;

    if (pc >= rlow && pc <= rhigh) {
    signal->connect(
        sigc::mem_fun(*this, &TranslationBlockTracer::onExecuteBlockStart)
    );
    }
}

void TranslationBlockTracer::onTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    uint64_t rlow = 0x4746c4 - 0x400000 + 0x804d7000;
    uint64_t rhigh = 0x474e23 - 0x400000 + 0x804d7000;

    if (endPc >= rlow && endPc <= rhigh) {
        signal->connect(
            sigc::mem_fun(*this, &TranslationBlockTracer::onExecuteBlockEnd)
        );
    }
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
    tb.size = state->getTb()->size;
    memset(tb.registers, 0x55, sizeof(tb.registers));

    assert(sizeof(tb.symbMask)*8 >= sizeof(tb.registers)/sizeof(tb.registers[0]));
    for (unsigned i=0; i<sizeof(tb.registers)/sizeof(tb.registers[0]); ++i) {
        //XXX: make it portable across architectures
    	//XXX: test for ARM
#ifdef TARGET_ARM
        unsigned offset = offsetof(CPUARMState, regs) + i*sizeof(tb.registers[0]);
#elif defined(TARGET_I386)
        unsigned offset = offsetof(CPUX86State, regs) + i*sizeof(tb.registers[0]);
#endif
        if (!state->readCpuRegisterConcrete(offset, &tb.registers[i], sizeof(tb.registers[0]))) {
            tb.registers[i]  = 0xDEADBEEF;
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
