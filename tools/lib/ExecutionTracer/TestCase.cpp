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
#include "TestCase.h"

using namespace s2e::plugins;

namespace s2etools {

TestCase::TestCase(LogEvents *events)
{
   m_connection = events->onEachItem.connect(
           sigc::mem_fun(*this, &TestCase::onItem));
   m_events = events;
}

TestCase::~TestCase()
{
    m_connection.disconnect();
}

void TestCase::onItem(unsigned traceIndex,
        const s2e::plugins::ExecutionTraceItemHeader &hdr,
        void *item)
{
    if (hdr.type != s2e::plugins::TRACE_TESTCASE) {
        return;
    }

    TestCaseState *state = static_cast<TestCaseState*>(m_events->getState(this, &TestCaseState::factory));

    std::cerr << "TestCase stateId=" << hdr.stateId << std::endl;
    if (state->m_foundInputs) {
        std::cerr << "The execution trace has multiple input sets. Make sure you used the PathBuilder filter."
                <<std::endl;
        assert(false);

    }
    ExecutionTraceTestCase::deserialize(item, hdr.size, state->m_inputs);
    state->m_foundInputs = true;

}

void TestCaseState::printInputsLine(std::ostream &os)
{
    if (!m_foundInputs) {
        os << "No concrete inputs found in the trace. Make sure you used the TestCaseGenerator plugin.";
        return;
    }

    ExecutionTraceTestCase::ConcreteInputs::iterator it;

    for (it = m_inputs.begin(); it != m_inputs.end(); ++it) {
        const ExecutionTraceTestCase::VarValuePair &vp = *it;
        //os << vp.first << ": ";

        for (unsigned i=0; i<vp.second.size(); ++i) {
            os << std::hex << std::setw(2) << std::right << std::setfill('0') << (unsigned) vp.second[i] << ' ';
        }

    }
}

void TestCaseState::printInputs(std::ostream &os)
{
    if (!m_foundInputs) {
        os << "No concrete inputs found in the trace. Make sure you used the TestCaseGenerator plugin." <<
                std::endl;
        return;
    }

    ExecutionTraceTestCase::ConcreteInputs::iterator it;
    os << "Concrete inputs:" << std::endl;

    for (it = m_inputs.begin(); it != m_inputs.end(); ++it) {
        const ExecutionTraceTestCase::VarValuePair &vp = *it;
        os << "  " << vp.first << ": ";

        for (unsigned i=0; i<vp.second.size(); ++i) {
            os << std::setw(2) << std::right << std::setfill('0') << (unsigned) vp.second[i] << ' ';
        }

        os << std::setfill(' ')<< std::endl;
    }
}

ItemProcessorState *TestCaseState::factory()
{
    return new TestCaseState();
}

TestCaseState::TestCaseState()
{
    m_foundInputs = false;
}

TestCaseState::~TestCaseState()
{

}

ItemProcessorState *TestCaseState::clone() const
{
    return new TestCaseState(*this);
}

}
