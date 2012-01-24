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

///XXX: Do not use, deprecated

extern "C" {
#include "config.h"
#include "qemu-common.h"
}

#include "InstructionCounter.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/Database.h>

#include <llvm/Support/TimeValue.h>

#include <iostream>
#include <sstream>



namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(InstructionCounter, "Instruction counter plugin", "InstructionCounter", "ExecutionTracer", "ModuleExecutionDetector");

void InstructionCounter::initialize()
{
    m_tb = NULL;

    m_executionTracer = static_cast<ExecutionTracer*>(s2e()->getPlugin("ExecutionTracer"));
    assert(m_executionTracer);

    m_executionDetector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));
    assert(m_executionDetector);

    //TODO: whole-system counting
    startCounter();
}




/////////////////////////////////////////////////////////////////////////////////////
void InstructionCounter::startCounter()
{
    m_executionDetector->onModuleTranslateBlockStart.connect(
            sigc::mem_fun(*this, &InstructionCounter::onTranslateBlockStart)
            );
}


/////////////////////////////////////////////////////////////////////////////////////

/**
 *  Instrument only the blocks where we want to count the instructions.
 */
void InstructionCounter::onTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t pc)
{
    if (m_tb) {
        m_tbConnection.disconnect();
    }
    m_tb = tb;

    CorePlugin *plg = s2e()->getCorePlugin();
    m_tbConnection = plg->onTranslateInstructionStart.connect(
            sigc::mem_fun(*this, &InstructionCounter::onTranslateInstructionStart)
    );

    //This function will flush the number of executed instructions
    signal->connect(
        sigc::mem_fun(*this, &InstructionCounter::onTraceTb)
    );
}


void InstructionCounter::onTranslateInstructionStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        TranslationBlock *tb,
        uint64_t pc)
{
    if (tb != m_tb) {
        //We've been suddenly interrupted by some other module
        m_tb = NULL;
        m_tbConnection.disconnect();
        return;
    }

    //Connect a function that will increment the number of executed
    //instructions.
    signal->connect(
        sigc::mem_fun(*this, &InstructionCounter::onTraceInstruction)
    );

}

void InstructionCounter::onModuleTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    //TRACE("%"PRIx64" StaticTarget=%d TargetPc=%"PRIx64"\n", endPc, staticTarget, targetPc);

    //Done translating the blocks, no need to instrument anymore.
    m_tb = NULL;
    m_tbConnection.disconnect();
}

/////////////////////////////////////////////////////////////////////////////////////

void InstructionCounter::onTraceTb(S2EExecutionState* state, uint64_t pc)
{
    //Get the plugin state for the current path
    DECLARE_PLUGINSTATE(InstructionCounterState, state);

    if (plgState->m_lastTbPc == pc) {
        //Avoid repeateadly tracing tight loops.
        return;
    }

    //Flush the counter
    ExecutionTraceICount e;
    e.count = plgState->m_iCount;
    m_executionTracer->writeData(state, &e, sizeof(e), TRACE_ICOUNT);
}


void InstructionCounter::onTraceInstruction(S2EExecutionState* state, uint64_t pc)
{
    //Get the plugin state for the current path
    DECLARE_PLUGINSTATE(InstructionCounterState, state);

    //Increment the instruction count
    plgState->m_iCount++;
}


/////////////////////////////////////////////////////////////////////////////////////
InstructionCounterState::InstructionCounterState()
{
    m_iCount = 0;
    m_lastTbPc = 0;
}

InstructionCounterState::InstructionCounterState(S2EExecutionState *s, Plugin *p)
{
    m_iCount = 0;
    m_lastTbPc = 0;
}

InstructionCounterState::~InstructionCounterState()
{

}

PluginState *InstructionCounterState::clone() const
{
    return new InstructionCounterState(*this);
}

PluginState *InstructionCounterState::factory(Plugin *p, S2EExecutionState *s)
{
    return new InstructionCounterState(s, p);
}

} // namespace plugins
} // namespace s2e


