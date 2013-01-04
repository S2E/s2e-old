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
 * All contributors are listed in the S2E-AUTHORS file.
 */

#include <iomanip>
#include <cctype>

#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/S2EExecutor.h>
#include "TestCaseGenerator.h"
#include "ExecutionTracer.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(TestCaseGenerator, "TestCaseGenerator plugin", "TestCaseGenerator", "ExecutionTracer");

TestCaseGenerator::TestCaseGenerator(S2E* s2e)
        : Plugin(s2e)
{
    m_testIndex = 0;
    m_pathsExplored = 0;

}

void TestCaseGenerator::initialize()
{
    s2e()->getCorePlugin()->onTestCaseGeneration.connect(
            sigc::mem_fun(*this, &TestCaseGenerator::onTestCaseGeneration));
}


void TestCaseGenerator::onTestCaseGeneration(S2EExecutionState *state, const std::string &message)
{
    s2e()->getMessagesStream()
            << "TestCaseGenerator: processTestCase of state " << state->getID()
            << " at address " << hexval(state->getPc())
            << '\n';

    ConcreteInputs out;
    bool success = s2e()->getExecutor()->getSymbolicSolution(*state, out);

    if (!success) {
        s2e()->getWarningsStream() << "Could not get symbolic solutions" << '\n';
        return;
    }

#if 0
    foreach2(it, state.constraints.begin(), state.constraints.end()) {
        s2e()->getMessagesStream() << "Constraint: " << std::hex << *it << '\n';
    }
#endif

    s2e()->getMessagesStream() << '\n';

    ExecutionTracer *tracer = (ExecutionTracer*)s2e()->getPlugin("ExecutionTracer");
    assert(tracer);

    std::stringstream ss;
    ConcreteInputs::iterator it;
    for (it = out.begin(); it != out.end(); ++it) {
        const VarValuePair &vp = *it;
        ss << std::setw(20) << vp.first << ": ";

        for (unsigned i = 0; i < vp.second.size(); ++i) {
            if (i != 0)
                ss << ' ';
            ss << std::setw(2) << std::setfill('0') << std::hex << (unsigned) vp.second[i] << std::dec;
        }
        ss << std::setfill(' ') << ", ";

        if (vp.second.size() == sizeof(int32_t)) {
            int32_t valueAsInt = vp.second[0] | ((int32_t)vp.second[1] << 8) | ((int32_t)vp.second[2] << 16) | ((int32_t)vp.second[3] << 24);
            ss << "(int32_t) " << valueAsInt << ", ";
        }
        if (vp.second.size() == sizeof(int64_t)) {
            int64_t valueAsInt = vp.second[0] | ((int64_t)vp.second[1] <<  8) | ((int64_t)vp.second[2] << 16) | ((int64_t)vp.second[3] << 24) |
                ((int64_t)vp.second[4] << 32) | ((int64_t)vp.second[5] << 40) | ((int64_t)vp.second[6] << 48) | ((int64_t)vp.second[7] << 56);
            ss << "(int64_t) " << valueAsInt << ", ";
        }

        ss << "(string) \"";
        for (unsigned i=0; i < vp.second.size(); ++i) {
            ss << (char)(std::isprint(vp.second[i]) ? vp.second[i] : '.');
        }
        ss << "\"\n";
    }

    s2e()->getMessagesStream() << ss.str();

    unsigned bufsize;
    ExecutionTraceTestCase *tc = ExecutionTraceTestCase::serialize(&bufsize, out);
    tracer->writeData(state, tc, bufsize, TRACE_TESTCASE);
    ExecutionTraceTestCase::deallocate(tc);
}

}
}
