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

#ifndef S2E_PLUGINS_MAXTBSEARCHER_H
#define S2E_PLUGINS_MAXTBSEARCHER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/S2EExecutionState.h>

#include <klee/Searcher.h>

#include <vector>

namespace s2e {
namespace plugins {

class MaxTbSearcher;

class MaxTbSearcherState: public PluginState
{
private:
    uint64_t m_metric;
    MaxTbSearcher *m_plugin;
    S2EExecutionState *m_state;
public:

    MaxTbSearcherState();
    MaxTbSearcherState(S2EExecutionState *s, Plugin *p);
    virtual ~MaxTbSearcherState();
    virtual PluginState *clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    friend class MaxTbSearcher;

};

class MaxTbSearcher : public Plugin, public klee::Searcher
{
    S2E_PLUGIN
public:
    struct MaxTbSorter{
        Plugin *p;

        MaxTbSorter() {
            p = NULL;
        }

        bool operator()(const S2EExecutionState *s1, const S2EExecutionState *s2) const{
            const MaxTbSearcherState *p1 = static_cast<MaxTbSearcherState*>(p->getPluginState(const_cast<S2EExecutionState*>(s1), &MaxTbSearcherState::factory));
            const MaxTbSearcherState *p2 = static_cast<MaxTbSearcherState*>(p->getPluginState(const_cast<S2EExecutionState*>(s2), &MaxTbSearcherState::factory));

            if (p1->m_metric == p2->m_metric) {
                return p1 < p2;
            }
            return p1->m_metric < p2->m_metric;
        }
    };

    typedef std::set<S2EExecutionState*, MaxTbSorter> StateSet;

    //Maps a translation block address to the number of times it was executed
    typedef std::map<uint64_t, uint64_t> TbMap;
    typedef std::map<ModuleDescriptor, TbMap, ModuleDescriptor::ModuleByName > TbsByModule;

    MaxTbSearcher(S2E* s2e): Plugin(s2e) {}
    void initialize();

    virtual klee::ExecutionState& selectState();
    virtual void update(klee::ExecutionState *current,
                        const std::set<klee::ExecutionState*> &addedStates,
                        const std::set<klee::ExecutionState*> &removedStates);

    virtual bool empty();

private:

    ModuleExecutionDetector *m_moduleExecutionDetector;
    bool m_searcherInited;

    klee::Searcher *m_parentSearcher;
    TbsByModule m_coveredTbs;

    MaxTbSorter m_sorter;
    StateSet m_states;


    void addTb(S2EExecutionState *s, uint64_t absTargetPc);
    bool isExplored(S2EExecutionState *s, uint64_t absTargetPc);
    uint64_t computeTargetPc(S2EExecutionState *s);
    bool updatePc(S2EExecutionState *es);

    void onModuleTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc);

    void onTraceTb(S2EExecutionState* state, uint64_t pc);

    void initializeSearcher();

    friend class MaxTbSearcherState;
};



} // namespace plugins
} // namespace s2e

#endif
