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

#ifndef S2E_PLUGINS_COOPSEARCHER_H
#define S2E_PLUGINS_COOPSEARCHER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/S2EExecutionState.h>

#include <klee/Searcher.h>

#include <set>

namespace s2e {
namespace plugins {


class ConcolicDFSSearcher : public Plugin, public klee::Searcher
{
    S2E_PLUGIN

    struct SortById {
        bool operator ()(const klee::ExecutionState *_s1, const klee::ExecutionState *_s2) const {
            const S2EExecutionState *s1 = static_cast<const S2EExecutionState*>(_s1);
            const S2EExecutionState *s2 = static_cast<const S2EExecutionState*>(_s2);

            return s1->getID() < s2->getID();
        }
    };

    typedef std::set<klee::ExecutionState*, SortById> States;

public:
    ConcolicDFSSearcher(S2E* s2e): Plugin(s2e) {}
    void initialize();

    virtual klee::ExecutionState& selectState();


    virtual void update(klee::ExecutionState *current,
                        const std::set<klee::ExecutionState*> &addedStates,
                        const std::set<klee::ExecutionState*> &removedStates);

    virtual bool empty();

private:

    States m_normalStates;
    States m_speculativeStates;
};



} // namespace plugins
} // namespace s2e

#endif
