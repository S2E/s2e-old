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

/**
 * This plugin implements a cooperative searcher.
 * The current state is run until the running program expicitely
 * asks to schedule another one, akin to cooperative scheduling.
 *
 * This searcher is useful for debugging S2E, becauses it allows
 * to control the sequence of executed states.
 *
 * RESERVES THE CUSTOM OPCODE 0xAB
 */

extern "C" {
#include "config.h"
#include "qemu-common.h"
}


#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/S2EExecutor.h>

#include <iostream>

#include "CooperativeSearcher.h"

namespace s2e {
namespace plugins {

using namespace llvm;

S2E_DEFINE_PLUGIN(CooperativeSearcher, "Uses custom instructions to schedule states",
                  "CooperativeSearcher");

void CooperativeSearcher::initialize()
{
    m_searcherInited = false;
    m_currentState = NULL;
    initializeSearcher();
}

void CooperativeSearcher::initializeSearcher()
{
    if (m_searcherInited) {
        return;
    }

    s2e()->getExecutor()->setSearcher(this);
    m_searcherInited = true;

    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &CooperativeSearcher::onCustomInstruction));

}



klee::ExecutionState& CooperativeSearcher::selectState()
{
    if (m_currentState) {
        return *m_currentState;
    }

    if (m_states.size() > 0) {
        return *(*m_states.begin()).second;
    }

    assert(false && "There are no states to select!");
}


void CooperativeSearcher::update(klee::ExecutionState *current,
                    const std::set<klee::ExecutionState*> &addedStates,
                    const std::set<klee::ExecutionState*> &removedStates)
{
    foreach2(it, removedStates.begin(), removedStates.end()) {
        S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
        m_states.erase(es->getID());

        if (m_currentState == es) {
            m_currentState = NULL;
        }
    }

    foreach2(it, addedStates.begin(), addedStates.end()) {
        S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
        m_states[es->getID()] = es;
    }

    if (m_currentState == NULL && m_states.size() > 0) {
        m_currentState = (*m_states.begin()).second;
    }
}

bool CooperativeSearcher::empty()
{
    return m_states.empty();
}

void CooperativeSearcher::onCustomInstruction(S2EExecutionState* state, uint64_t opcode)
{
    //XXX: find a better way of allocating custom opcodes
    if (((opcode>>8) & 0xFF) != COOPSEARCHER_OPCODE) {
        return;
    }

    //XXX: remove this mess. Should have a function for extracting
    //info from opcodes.
    opcode >>= 16;
    uint8_t op = opcode & 0xFF;
    opcode >>= 8;

    bool ok = true;
    uint32_t nextState = 0;

    CoopSchedulerOpcodes opc = (CoopSchedulerOpcodes)op;
    switch(opc) {
        //Pick the next state specified by the EAX register
        case ScheduleNext:
        {
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &nextState, 4);
            if(!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op "
                       "CooperativeSearcher ScheduleNext" << '\n';
                break;
            }

            States::iterator it = m_states.find(nextState);
            if (it == m_states.end()) {
                s2e()->getWarningsStream(state)
                    << "ERROR: Invalid state passed to " <<
                    "CooperativeSearcher ScheduleNext: " << nextState << '\n';
            }

            m_currentState = (*it).second;

            s2e()->getMessagesStream(state) <<
                    "CooperativeSearcher picked the state " << nextState << '\n';

            //Force rescheduling
            state->writeCpuState(CPU_OFFSET(eip), state->getPc() + 10, 32);
            throw CpuExitException();
            break;
        }

        //Deschedule the current state. Will pick the state with strictly lower id.
        case Yield:
        {
            if (m_states.size() == 1) {
                break;
            }

            States::iterator it = m_states.find(m_currentState->getID());
            if (it == m_states.begin()) {
                m_currentState = (*m_states.rbegin()).second;
            }else {
                --it;
                m_currentState = (*it).second;
            }

            //Force rescheduling
            state->writeCpuState(CPU_OFFSET(eip), state->getPc() + 10, 32);
            throw CpuExitException();
            break;
        }
    }
}


} // namespace plugins
} // namespace s2e
