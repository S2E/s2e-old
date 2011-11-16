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
#include "qemu-common.h"
}

#include "MaxTbSearcher.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/S2EExecutor.h>

#include <iostream>

#include <llvm/Instructions.h>
#include <llvm/Constants.h>
#include <llvm/Function.h>

namespace s2e {
namespace plugins {

using namespace llvm;

S2E_DEFINE_PLUGIN(MaxTbSearcher, "Prioritizes states that are about to execute unexplored translation blocks",
                  "MaxTbSearcher", "ModuleExecutionDetector");

void MaxTbSearcher::initialize()
{

    m_moduleExecutionDetector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));
    m_searcherInited = false;
    m_parentSearcher = NULL;

    m_sorter.p = this;
    m_states = StateSet(m_sorter);

    //XXX: Take care of module load/unload
    m_moduleExecutionDetector->onModuleTranslateBlockEnd.connect(
            sigc::mem_fun(*this, &MaxTbSearcher::onModuleTranslateBlockEnd)
            );

}

void MaxTbSearcher::initializeSearcher()
{
    if (m_searcherInited) {
        return;
    }

    m_parentSearcher = s2e()->getExecutor()->getSearcher();
    assert(m_parentSearcher);
    s2e()->getExecutor()->setSearcher(this);
    m_searcherInited = true;

}

bool MaxTbSearcher::isExplored(S2EExecutionState *s, uint64_t absTargetPc)
{
    const ModuleDescriptor* md = m_moduleExecutionDetector->getCurrentDescriptor(s);
    assert(md);

    uint64_t targetPc = md->ToNativeBase(absTargetPc);
    TbsByModule::iterator it = m_coveredTbs.find(*md);
    if (it == m_coveredTbs.end()) {
        return false;
    }else {
        if ((*it).second.find(targetPc) == (*it).second.end()) {
            return false;
        }
    }
    return true;
}

#if 0
void MaxTbSearcher::addTb(S2EExecutionState *s, uint64_t absTargetPc)
{
    const ModuleDescriptor* md = m_moduleExecutionDetector->getCurrentDescriptor(s);
    assert(md);

    uint64_t targetPc = md->ToNativeBase(absTargetPc);
    m_coveredTbs[*md].insert(targetPc);
}


static uint64_t GetPcAssignment(const BasicBlock *bb)
{
    const TerminatorInst *term = bb->getTerminator();
    assert(term);

    if (!dyn_cast<ReturnInst>(term)) {
        return false;
    }

    //The store instruction changing the program counter must be the
    //third instruction from the end of the block
    unsigned Idx = 0;
    const BasicBlock::InstListType &InstList = bb->getInstList();
    foreach2(iit, InstList.rbegin(), InstList.rend()) {
      if (const StoreInst *Store = dyn_cast<StoreInst>(&*iit)) {
        if (Idx == 1) {
          const Value *v = Store->getOperand(0);
          if (const ConstantInt *Ci = dyn_cast<ConstantInt>(v)) {
            const uint64_t* Int = Ci->getValue().getRawData();
            return *Int;
          }
        }
        Idx ++;
      }
    }
    return 0;
}
#endif

uint64_t MaxTbSearcher::computeTargetPc(S2EExecutionState *state)
{
    const Instruction *instr = state->pc->inst;

    //Check whether we are the first instruction of the block
    const BasicBlock *BB = instr->getParent();
    if (instr != &*BB->begin()) {
        return 0;
    }

    //There can be only one predecessor jumping to the terminating block (xxx: check this)
    const BasicBlock *PredBB = BB->getSinglePredecessor();
    if (!PredBB) {
        return 0;
    }

    const BranchInst *Bi = dyn_cast<BranchInst>(PredBB->getTerminator());
    if (!Bi) {
        return 0;
    }

    //instr must be a call to tcg_llvm_fork_and_concretize
    s2e()->getDebugStream() << "MaxTbSearcher: " << *instr << '\n';
       
    const CallInst *callInst = dyn_cast<CallInst>(instr);
    if (!callInst) {
        return 0;
    }

    assert(callInst->getCalledFunction()->getName() == "tcg_llvm_fork_and_concretize");

    const ConstantInt *Ci = dyn_cast<ConstantInt>(callInst->getOperand(1));
    if (!Ci) {
        return false;
    }

    const uint64_t* Int = Ci->getValue().getRawData();
    return *Int;
    //return GetPcAssignment(BB);
}


/**
 *  Prioritize the current state while it keeps discovering new blocks.
 *  XXX: the implementation can add the state even if the targetpc is not reachable
 *  because of path constraints.
 */
void MaxTbSearcher::onModuleTranslateBlockEnd(
    ExecutionSignal *signal,
    S2EExecutionState* state,
    const ModuleDescriptor &,
    TranslationBlock *tb,
    uint64_t endPc,
    bool staticTarget,
    uint64_t targetPc)
{
    initializeSearcher();

    /*if (staticTarget) {
        if (!isExplored(state, targetPc)) {
            m_states.insert(state);
        }
    }*/
    signal->connect(
         sigc::mem_fun(*this, &MaxTbSearcher::onTraceTb)
    );
}



//Update the metrics
void MaxTbSearcher::onTraceTb(S2EExecutionState* state, uint64_t pc)
{
    //Increment the number of times the current tb is executed
    const ModuleDescriptor *curModule = m_moduleExecutionDetector->getModule(state, pc);
    assert(curModule);

    const ModuleDescriptor *md = m_moduleExecutionDetector->getCurrentDescriptor(state);

    uint64_t tbVa = curModule->ToRelative(state->getTb()->pc);

    if (!md) {
        m_states.erase(state);
        m_coveredTbs[*curModule][tbVa]++;
        DECLARE_PLUGINSTATE(MaxTbSearcherState, state);
        plgState->m_metric = m_coveredTbs[*curModule][tbVa];
        plgState->m_metric *= state->queryCost < 1 ? 1 : state->queryCost;
        m_states.insert(state);
        return;
    }

    uint64_t newPc = md->ToRelative(state->getPc());


    TbMap &tbm = m_coveredTbs[*md];
    TbMap::iterator NewTbIt = tbm.find(newPc);
    TbMap::iterator CurTbIt = tbm.find(tbVa);

    unsigned CurTbFreq = 0;
    //unsigned NewTbFreq = 0;

    bool NextTbIsNew = NewTbIt == tbm.end();
    bool CurTbIsNew = CurTbIt == tbm.end();

    m_states.erase(state);

    /**
     * Update the frequency of the current and next
     * translation blocks
     */
    if (CurTbIsNew) {
      tbm[tbVa] = 1;
      CurTbFreq = 1;
    }else {
      CurTbFreq = (*CurTbIt).second + 1;
      (*CurTbIt).second++;
    }

    if (NextTbIsNew && md) {
      tbm[newPc] = 0;
    }

    if (NextTbIsNew) {
      tbm[newPc] = 0;
    }

    DECLARE_PLUGINSTATE(MaxTbSearcherState, state);
    plgState->m_metric = tbm[newPc];

#if 1
    s2e()->getDebugStream() << "Metric for " << hexval(newPc+md->NativeBase) << " = " << plgState->m_metric
            << '\n';
#endif

    plgState->m_metric *= state->queryCost < 1 ? 1 : state->queryCost;

    m_states.insert(state);
}

klee::ExecutionState& MaxTbSearcher::selectState()
{
    //If there are no prioritized states, revert to the parent searcher
    StateSet::iterator it;

#if 0
    uint64_t absNextPc = 0;
    while((it = m_states.begin()) != m_states.end()) {
        S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
        m_states.erase(es);
        ++it;

        //Get the program counter of the selected state, and add it
        //to the list of explored translation blocks
        absNextPc = computeTargetPc(es);
        if(absNextPc != 0) {
            s2e()->getDebugStream() << "MaxTbSearcher: selected state going to 0x" << std::hex << absNextPc << '\n';
            addTb(es, absNextPc);
            return *es;
        }
    }while(absNextPc);
#endif

    if (m_states.size() > 0) {
        S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*m_states.begin());
        DECLARE_PLUGINSTATE(MaxTbSearcherState, es);
        if (plgState->m_metric < 2) {
            return *es;
        }

    }

    return m_parentSearcher->selectState();

}

bool MaxTbSearcher::updatePc(S2EExecutionState *es)
{
    const ModuleDescriptor* md = m_moduleExecutionDetector->getCurrentDescriptor(es);
    if (!md) {
        return false;
    }

    //Retrieve the next program counter of the state
    uint64_t absNextPc = computeTargetPc(es); //XXX: fix me

    if (!absNextPc) {
        s2e()->getDebugStream() << "MaxTBSearcher: could not determune next pc" << '\n';
        //Could not determine next pc
        return false;
    }

    DECLARE_PLUGINSTATE(MaxTbSearcherState, es);

    //If not covered, add the forked state to the wait list
    plgState->m_metric = m_coveredTbs[*md][md->ToRelative(absNextPc)];
#if 1
    s2e()->getDebugStream() << "MaxTBSearcher updatePc Metric for " << hexval(md->ToNativeBase(absNextPc)) << " = " << plgState->m_metric
            << '\n';
#endif

    m_states.insert(es);
    return true;
}

void MaxTbSearcher::update(klee::ExecutionState *current,
                    const std::set<klee::ExecutionState*> &addedStates,
                    const std::set<klee::ExecutionState*> &removedStates)
{
    m_parentSearcher->update(current, addedStates, removedStates);

    foreach2(it, removedStates.begin(), removedStates.end()) {
        S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
        m_states.erase(es);
    }

    foreach2(it, addedStates.begin(), addedStates.end()) {
        S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
        updatePc(es);
    }
}

bool MaxTbSearcher::empty()
{
    if (!m_states.empty()) {
        return false;
    }

    return m_parentSearcher->empty();
}


MaxTbSearcherState::MaxTbSearcherState()
{

}

MaxTbSearcherState::MaxTbSearcherState(S2EExecutionState *s, Plugin *p)
{
    m_metric = 0;
    m_plugin = static_cast<MaxTbSearcher*>(p);
    m_state = s;
}

MaxTbSearcherState::~MaxTbSearcherState()
{
}

PluginState *MaxTbSearcherState::clone() const
{
    return new MaxTbSearcherState(*this);
}

PluginState *MaxTbSearcherState::factory(Plugin *p, S2EExecutionState *s)
{
    MaxTbSearcherState *ret = new MaxTbSearcherState(s, p);
    return ret;
}

} // namespace plugins
} // namespace s2e
