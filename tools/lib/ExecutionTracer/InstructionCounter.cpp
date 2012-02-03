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

#include <iomanip>
#include <iostream>
#include <cassert>
#include "InstructionCounter.h"

using namespace s2e::plugins;

namespace s2etools {

InstructionCounter::InstructionCounter(LogEvents *events)
{
   m_events = events;
   m_connection = events->onEachItem.connect(
           sigc::mem_fun(*this, &InstructionCounter::onItem));
}

InstructionCounter::~InstructionCounter()
{
    m_connection.disconnect();
}

void InstructionCounter::onItem(unsigned traceIndex,
        const s2e::plugins::ExecutionTraceItemHeader &hdr,
        void *item)
{
    if (hdr.type != s2e::plugins::TRACE_ICOUNT) {
        return;
    }

    ExecutionTraceICount *e = static_cast<ExecutionTraceICount*>(item);
    InstructionCounterState *state = static_cast<InstructionCounterState*>(m_events->getState(this, &InstructionCounterState::factory));

    #ifdef DEBUG_PB
    std::cout << "ID=" << traceIndex << " ICOUNT: e=" << e->count << " state=" << state->m_icount <<
                 " item=" << item << std::endl;
    #endif

    assert(e->count >= state->m_icount);
    state->m_icount = e->count;
}

void InstructionCounterState::printCounter(std::ostream &os)
{
    os << "Instruction count: " << std::dec << m_icount << std::endl;
}

ItemProcessorState *InstructionCounterState::factory()
{
    return new InstructionCounterState();
}

InstructionCounterState::InstructionCounterState()
{
   m_icount = 0;
}

InstructionCounterState::~InstructionCounterState()
{

}

ItemProcessorState *InstructionCounterState::clone() const
{
    return new InstructionCounterState(*this);
}

}
