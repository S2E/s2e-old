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
    extern struct CPUX86State *env;
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

    bool ok = false;
    //Specify whether or not to enable cutom instructions for enabling/disabling tracing
    bool manualTrigger = s2e()->getConfig()->getBool(getConfigKey() + ".manualTrigger", false, &ok);

    //Whether or not to flush the translation block cache when enabling/disabling tracing.
    //This can be useful when tracing is enabled in the middle of a run where most of the blocks
    //are already translated without the tracing instrumentation enabled.
    //The default behavior is ON, because otherwise it may produce confising results.
    m_flushTbOnChange = s2e()->getConfig()->getBool(getConfigKey() + ".flushTbCache", true);

    if (manualTrigger) {
        s2e()->getCorePlugin()->onCustomInstruction.connect(
                sigc::mem_fun(*this, &TranslationBlockTracer::onCustomInstruction));
    }else {
        enableTracing();
    }
}

void TranslationBlockTracer::enableTracing()
{
    if (m_flushTbOnChange) {
        tb_flush(env);
    }

    m_tbStartConnection = m_detector->onModuleTranslateBlockStart.connect(
            sigc::mem_fun(*this, &TranslationBlockTracer::onModuleTranslateBlockStart)
    );

    m_tbEndConnection = m_detector->onModuleTranslateBlockEnd.connect(
            sigc::mem_fun(*this, &TranslationBlockTracer::onModuleTranslateBlockEnd)
    );
}

void TranslationBlockTracer::disableTracing()
{
    if (m_flushTbOnChange) {
        tb_flush(env);
    }

    m_tbStartConnection.disconnect();
    m_tbEndConnection.disconnect();
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

    if (type == TRACE_TB_START) {
        if (pc != state->getTb()->pc) {
            s2e()->getWarningsStream() << "BUG! pc=" << hexval(pc)
                                       << " tbpc=" << hexval(state->getTb()->pc) << '\n';
            exit(-1);
        }
    }

    tb.pc = pc;
    tb.targetPc = state->getPc();
    tb.tbType = state->getTb()->s2e_tb_type;
    tb.symbMask = 0;
    tb.size = state->getTb()->size;
    memset(tb.registers, 0x55, sizeof(tb.registers));

    assert(sizeof(tb.symbMask)*8 >= sizeof(tb.registers)/sizeof(tb.registers[0]));
    for (unsigned i=0; i<sizeof(tb.registers)/sizeof(tb.registers[0]); ++i) {
        //XXX: make it portable across architectures
        unsigned offset = offsetof(CPUX86State, regs) + i*sizeof(tb.registers[0]);
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

void TranslationBlockTracer::onCustomInstruction(S2EExecutionState* state, uint64_t opcode)
{
    //XXX: find a better way of allocating custom opcodes
    if (((opcode>>8) & 0xFF) != TB_TRACER_OPCODE) {
        return;
    }

    //XXX: remove this mess. Should have a function for extracting
    //info from opcodes.
    opcode >>= 16;
    uint8_t op = opcode & 0xFF;
    opcode >>= 8;


    TbTracerOpcodes opc = (TbTracerOpcodes)op;
    switch(opc) {
    case Enable:
        enableTracing();
        break;

    case Disable:
        disableTracing();
        break;

    default:
        s2e()->getWarningsStream() << "MemoryTracer: unsupported opcode " << hexval(opc) << '\n';
        break;
    }

}

} // namespace plugins
} // namespace s2e
