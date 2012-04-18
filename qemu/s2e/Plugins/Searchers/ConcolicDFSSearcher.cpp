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

#include "ConcolicDFSSearcher.h"

namespace s2e {
namespace plugins {

using namespace llvm;

S2E_DEFINE_PLUGIN(ConcolicDFSSearcher, "Depth-first searcher, prioritizing non-speculative paths first",
                  "ConcolicDFSSearcher");

void ConcolicDFSSearcher::initialize()
{
    s2e()->getExecutor()->setSearcher(this);
}


klee::ExecutionState& ConcolicDFSSearcher::selectState()
{
    if (!m_normalStates.empty()) {
        return **m_normalStates.begin();
    }
    assert(!m_speculativeStates.empty());
    return **m_speculativeStates.begin();
}


void ConcolicDFSSearcher::update(klee::ExecutionState *current,
                    const std::set<klee::ExecutionState*> &addedStates,
                    const std::set<klee::ExecutionState*> &removedStates)
{
    if (current) {
        if (current->isSpeculative()) {
            m_normalStates.erase(current);
            m_speculativeStates.insert(current);
        } else  {
            m_speculativeStates.erase(current);
            m_normalStates.insert(current);
        }
    }

    foreach2(it, removedStates.begin(), removedStates.end()) {
        S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
        if (es->isSpeculative()) {
            m_speculativeStates.erase(es);
        } else {
            m_speculativeStates.insert(es);
        }
    }

    foreach2(it, addedStates.begin(), addedStates.end()) {
        S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
        if (es->isSpeculative()) {
            m_speculativeStates.erase(es);
        } else {
            m_speculativeStates.insert(es);
        }
    }
}

bool ConcolicDFSSearcher::empty()
{
    return m_normalStates.empty() && m_speculativeStates.empty();
}


} // namespace plugins
} // namespace s2e
