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

#include "ExecutionTracer.h"

#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <llvm/Support/TimeValue.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(ExecutionTracer, "ExecutionTracer plugin", "",);

void ExecutionTracer::initialize()
{
    createNewTraceFile(false);

    s2e()->getCorePlugin()->onStateFork.connect(
            sigc::mem_fun(*this, &ExecutionTracer::onFork));

    s2e()->getCorePlugin()->onTimer.connect(
        sigc::mem_fun(*this, &ExecutionTracer::onTimer)
    );

    s2e()->getCorePlugin()->onProcessFork.connect(
        sigc::mem_fun(*this, &ExecutionTracer::onProcessFork)
    );
}

ExecutionTracer::~ExecutionTracer()
{
    if (m_LogFile) {
        fclose(m_LogFile);
    }
}

void ExecutionTracer::createNewTraceFile(bool append)
{

    if (append) {
        assert(m_fileName.size() > 0);
        m_LogFile = fopen(m_fileName.c_str(), "a");
    }else {
        m_fileName = s2e()->getOutputFilename("ExecutionTracer.dat");
        m_LogFile = fopen(m_fileName.c_str(), "wb");
    }

    if (!m_LogFile) {
        s2e()->getWarningsStream() << "Could not create ExecutionTracer.dat" << '\n';
        exit(-1);
    }
    m_CurrentIndex = 0;
}

void ExecutionTracer::onTimer()
{
    if (m_LogFile) {
        fflush(m_LogFile);
    }
}

uint32_t ExecutionTracer::writeData(
        const S2EExecutionState *state,
        void *data, unsigned size, ExecTraceEntryType type)
{
    ExecutionTraceItemHeader item;

    assert(m_LogFile);

    item.timeStamp = llvm::sys::TimeValue::now().usec();
    item.size = size;
    item.type = type;
    item.stateId = state->getID();
    item.pid = state->getPid();

    if (fwrite(&item, sizeof(item), 1, m_LogFile) != 1) {
        return 0;
    }

    if (size) {
        if (fwrite(data, size, 1, m_LogFile) != 1) {
            //at this point the log is corrupted.
            assert(false);
        }
    }

    return ++m_CurrentIndex;
}

void ExecutionTracer::flush()
{
    if (m_LogFile) {
        fflush(m_LogFile);
    }
}

void ExecutionTracer::onProcessFork(bool preFork, bool isChild, unsigned parentProcId)
{
    if (preFork) {
        fclose(m_LogFile);
        m_LogFile = NULL;
    }else {
        if (isChild) {
            createNewTraceFile(false);
        }else {
            createNewTraceFile(true);
        }
    }
}

void ExecutionTracer::onFork(S2EExecutionState *state,
            const std::vector<S2EExecutionState*>& newStates,
            const std::vector<klee::ref<klee::Expr> >& newConditions
            )
{
    assert(newStates.size() > 0);

    unsigned itemSize = sizeof(ExecutionTraceFork) +
                        (newStates.size()-1) * sizeof(uint32_t);

    uint8_t *itemBytes = new uint8_t[itemSize];
    ExecutionTraceFork *itemFork = reinterpret_cast<ExecutionTraceFork*>(itemBytes);

    itemFork->pc = state->getPc();
    itemFork->stateCount = newStates.size();


    for (unsigned i=0; i<newStates.size(); i++) {
        itemFork->children[i] = newStates[i]->getID();
    }

    writeData(state, itemFork, itemSize, TRACE_FORK);

    delete [] itemBytes;
}


} // namespace plugins
} // namespace s2e
