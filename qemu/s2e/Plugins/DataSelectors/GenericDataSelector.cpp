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


#include "GenericDataSelector.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>


#include <iostream>
#include <sstream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(GenericDataSelector, "Generic symbolic injection framework", 
                  "GenericDataSelector", "Interceptor", "ModuleExecutionDetector");

//WindowsService-specific initialization here
void GenericDataSelector::initialize()
{
    m_Monitor = (OSMonitor*)s2e()->getPlugin("Interceptor");
    assert(m_Monitor);
    m_tb = NULL;

    //Read the cfg file and call init sections
    DataSelector::initialize();

    m_Monitor->onModuleLoad.connect(
            sigc::mem_fun(*this, &GenericDataSelector::onModuleLoad)
            );

    m_Monitor->onModuleUnload.connect(
            sigc::mem_fun(*this, &GenericDataSelector::onModuleUnload)
            );

    m_Monitor->onProcessUnload.connect(
            sigc::mem_fun(*this, &GenericDataSelector::onProcessUnload)
            );

    //Registering listener
    m_ExecDetector->onModuleTranslateBlockStart.connect(
        sigc::mem_fun(*this, &GenericDataSelector::onTranslateBlockStart)
    );

    m_ExecDetector->onModuleTranslateBlockEnd.connect(
        sigc::mem_fun(*this, &GenericDataSelector::onTranslateBlockEnd)
    );
}

bool GenericDataSelector::initSection(const std::string &cfgKey, const std::string &svcId)
{
    RuleCfg cfg;
    
    bool ok;
    cfg.moduleId = s2e()->getConfig()->getString(cfgKey + ".moduleId", "", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".moduleId\n";
        return false;
    }

    if (!m_ExecDetector->isModuleConfigured(cfg.moduleId)) {
        s2e()->getWarningsStream() << cfg.moduleId << " is not configured in " << cfgKey  << '\n';
        exit(-1);
        return false;
    }

    std::string rule = s2e()->getConfig()->getString(cfgKey + ".rule", "", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".rule" << '\n';
        return false;
    }

    if (rule == "injectreg") {
        cfg.rule = RuleCfg::INJECTREG;
    }else if (rule == "injectmain") {
        cfg.rule = RuleCfg::INJECTMAIN;
    }else if (rule == "injectrsa") {
        cfg.rule = RuleCfg::RSAKEYGEN;
    }else if (rule == "injectmem") {
        cfg.rule = RuleCfg::INJECTMEM;
    }else {
        s2e()->getWarningsStream() << "Invalid rule " << rule << '\n';
        exit(-1);
        return false;
    }

    if (cfg.rule == RuleCfg::INJECTMEM) {
        cfg.size = s2e()->getConfig()->getInt(cfgKey + ".size", 0, &ok);
        if (!ok || cfg.size > 8 || cfg.size < 1) {
            s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".size with injectmem (between 1 and 8)\n";
            exit(-1);
            return false;
        }

        cfg.offset = s2e()->getConfig()->getInt(cfgKey + ".offset", 0, &ok);
        if (!ok) {
            s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".offset with injectmem\n";
            exit(-1);
            return false;
        }
    }

    cfg.pc = s2e()->getConfig()->getInt(cfgKey + ".pc", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".pc\n";
        exit(-1);
        return false;
    }

    cfg.concrete = s2e()->getConfig()->getBool(cfgKey + ".concrete");

    cfg.value = s2e()->getConfig()->getInt(cfgKey + ".value", 0, &ok);
    if (!ok) {
        if (cfg.concrete) {
            s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".value when injecting concrete values.\n";
            exit(-1);
            return false;
        }
    }

    cfg.reg = s2e()->getConfig()->getInt(cfgKey + ".register", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You might have forgotten to specify " << cfgKey <<  ".register\n";
        exit(-1);
        return false;
    }

    if (cfg.reg >= 8) {
        s2e()->getWarningsStream() << "You must specifiy a register between 0 and 7 in " << cfgKey <<  ".register\n";
        exit(-1);
        return false;
    }

    cfg.makeParamsSymbolic = s2e()->getConfig()->getBool(cfgKey + ".makeParamsSymbolic");
    cfg.makeParamCountSymbolic = s2e()->getConfig()->getBool(cfgKey + ".makeParamCountSymbolic");

    m_Rules.insert(cfg);
    return true;
}

//Activate all the relevant rules for each module
void GenericDataSelector::onModuleLoad(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    foreach2(it, m_Rules.begin(), m_Rules.end()) {
        const RuleCfg &cfg = *it;
        const std::string *s = m_ExecDetector->getModuleId(module);
        if (!s || (cfg.moduleId != *s)) {
            continue;
        }

        DECLARE_PLUGINSTATE(GenericDataSelectorState, state);

        if (cfg.pc - module.NativeBase > module.Size) {
            s2e()->getWarningsStream() << "Specified pc for rule exceeds the size of the loaded module\n";
        }

        plgState->activateRule(cfg, module);
    }
}

//Deactivate irrelevant rules
void GenericDataSelector::onModuleUnload(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    DECLARE_PLUGINSTATE(GenericDataSelectorState, state);
    plgState->deactivateRule(module);
}

void GenericDataSelector::onProcessUnload(
        S2EExecutionState* state,
        uint64_t pid
        )
{
    DECLARE_PLUGINSTATE(GenericDataSelectorState, state);
    plgState->deactivateRule(pid);
}

void GenericDataSelector::onTranslateBlockStart(
        ExecutionSignal* signal,
        S2EExecutionState *state,
        const ModuleDescriptor &desc,
        TranslationBlock *tb, uint64_t pc)
{
    //activateRule(signal, state, desc, tb, pc);

    if (m_tb) {
        m_tbConnection.disconnect();
    }
    m_tb = tb;

    /*if (desc.ToNativeBase(pc) == 0x124f6) {
        asm("int $3");
    }*/

    CorePlugin *plg = s2e()->getCorePlugin();
    m_tbConnection = plg->onTranslateInstructionStart.connect(
            sigc::mem_fun(*this, &GenericDataSelector::onTranslateInstructionStart)
    );


}

void GenericDataSelector::onTranslateInstructionStart(
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

    DECLARE_PLUGINSTATE(GenericDataSelectorState, state);

    //Connect if some injector is interested
    if (plgState->check(m_Monitor->getPid(state, pc), pc)) {
        signal->connect(
            sigc::mem_fun(*this, &GenericDataSelector::onExecution)
        );
    }

}


void GenericDataSelector::onTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &desc,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    //activateRule(signal, state, desc, tb, endPc);
    m_tb = NULL;
    m_tbConnection.disconnect();
}

 
void GenericDataSelector::onExecution(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(GenericDataSelectorState, state);
    const RuntimeRule &r = plgState->getRule(m_Monitor->getPid(state, pc), pc);

    switch(r.rule) {
        case RuleCfg::INJECTMAIN: injectMainArgs(state, &r); break;
        case RuleCfg::INJECTREG: injectRegister(state, pc, r.reg, r.concrete, r.value); break;
        case RuleCfg::INJECTMEM: injectMemory(state, pc, r.reg, r.offset, r.concrete, r.size, r.value); break;
        case RuleCfg::RSAKEYGEN: injectRsaGenKey(state); break;
        default: s2e()->getWarningsStream() << "Invalid rule type " << r.rule <<  '\n';
            break;
    }
}

void GenericDataSelector::injectMainArgs(S2EExecutionState *state, const RuntimeRule *rule)
{
    //Parse the arguments here
    uint32_t paramCount;
    uint32_t paramsArray;
    
    s2e()->getDebugStream() << "Injecting main() arguments\n";
    //XXX: hard-coded pointer size assumptions
    SREAD(state, state->getSp()+sizeof(uint32_t), paramCount);
    SREAD(state, state->getSp()+2*sizeof(uint32_t), paramsArray);
    
    s2e()->getMessagesStream() << "main paramCount="  <<
        paramCount << " - " << hexval(paramsArray) << "esp=" << hexval(state->getSp()) << '\n';

    for(unsigned i=0; i<paramCount; i++) {
        uint32_t paramPtr;
        std::string param;
        SREAD(state, paramsArray+i*sizeof(uint32_t), paramPtr);
        if (!state->readString(paramPtr, param)) {
            continue;
        }
        s2e()->getMessagesStream() << "main param" << i << " - " <<
            param << '\n';

        if (rule->makeParamsSymbolic) {
            makeStringSymbolic(state, paramPtr);
        }
    }

    //Make number of params symbolic
    if (rule->makeParamCountSymbolic) {
        klee::ref<klee::Expr> v = getUpperBound(state, paramCount, klee::Expr::Int32);
        s2e()->getMessagesStream() << "ParamCount is now " << v << '\n';
        state->writeMemory(state->getSp()+sizeof(uint32_t), v);
    }
}

void GenericDataSelector::injectRegister(S2EExecutionState *state, uint64_t pc, unsigned reg, bool concrete, uint32_t val)
{
    if (!concrete && state->needToJumpToSymbolic()) {
        //We  must update the pc here, because instruction handlers are called before the
        //program counter is updated by the generated code. Otherwise the previous instruction
        //will be reexecuted twice.
        state->setPc(pc);
        state->jumpToSymbolicCpp();
    }

    if (concrete) {
        s2e()->getDebugStream() << "Injecting concrete value " << hexval(val) << " in register " << reg
                << " at pc " << hexval(state->getPc()) << '\n';
        //state->dumpStack(20);
        assert (reg < 8);
        state->writeCpuRegisterConcrete(CPU_OFFSET(regs) + reg * sizeof(target_ulong), &val, sizeof(val));
    }else {
        s2e()->getDebugStream() << "Injecting symbolic value in register " << reg
                << " at pc " << hexval(state->getPc()) << '\n';

        assert (reg < 8);
        //state->dumpStack(20);
        klee::ref<klee::Expr> symb = state->createSymbolicValue(__FUNCTION__, klee::Expr::Int32);
        state->writeCpuRegister(CPU_OFFSET(regs) + reg * sizeof(target_ulong), symb);
    }
}

void GenericDataSelector::injectMemory(S2EExecutionState *state, uint64_t pc,
                                       unsigned reg,
                                       uint32_t offset,
                                       unsigned size,
                                       bool concrete, uint32_t val)
{
    if (!concrete && state->needToJumpToSymbolic()) {
        //We  must update the pc here, because instruction handlers are called before the
        //program counter is updated by the generated code. Otherwise the previous instruction
        //will be reexecuted twice.
        state->setPc(pc);
        state->jumpToSymbolicCpp();
    }

    llvm::raw_ostream &os = s2e()->getDebugStream();

    //Fetch the specified base register
    uint32_t base;
    if (!state->readCpuRegisterConcrete(CPU_OFFSET(regs) + reg * sizeof(target_ulong), &base, sizeof(base))) {
        os << "Failed to read base register " << reg << ". Make sure it is concrete!\n";
        return;
    }

    base += offset;
    assert (size <= 8);

    if (concrete) {
        os << "Injecting concrete value " << hexval(val) << " in memory " << hexval(base)
                << " at pc " << hexval(state->getPc()) << '\n';
        //state->dumpStack(20);
        assert (reg < 8);
        state->writeMemoryConcrete((uint64_t)base, &val, size);
     }else {
         s2e()->getDebugStream() << "Injecting symbolic value in memory " << hexval(base)
                << " at pc " << hexval(state->getPc()) << '\n';

        assert (reg < 8);
        //state->dumpStack(20);
        klee::ref<klee::Expr> symb = state->createSymbolicValue(__FUNCTION__, size*8);
        state->writeMemory(base, symb);
    }
}


//Adhoc injectors, should be replaced with KQuery expressions...
void GenericDataSelector::injectRsaGenKey(S2EExecutionState *state)
{
    uint32_t origKeySize;
    uint32_t origExponent;
    uint32_t origCallBack;
    
    SREAD(state, state->getSp()+sizeof(uint32_t), origKeySize);
    SREAD(state, state->getSp()+2*sizeof(uint32_t), origExponent);
    SREAD(state, state->getSp()+3*sizeof(uint32_t), origCallBack);

    s2e()->getMessagesStream() << "Caught RSA_generate_key" << '\n' <<
        "origKeySize=" << origKeySize << " origExponent=" << origExponent <<
        " origCallBack=" << hexval(origCallBack) << '\n';

    //Now we replace the arguments with properly constrained symbolic values
    klee::ref<klee::Expr> newKeySize = getUpperBound(state, 2048, klee::Expr::Int32);
    klee::ref<klee::Expr> newExponent = getOddValue(state, klee::Expr::Int32, 65537);

    s2e()->getMessagesStream() << "newKeySize=" << newKeySize << '\n';
    s2e()->getMessagesStream() << "newExponent=" << newExponent << '\n';

    state->writeMemory(state->getSp()+sizeof(uint32_t), newKeySize);
    state->writeMemory(state->getSp()+2*sizeof(uint32_t), newExponent);
}



GenericDataSelectorState::GenericDataSelectorState()
{

}

GenericDataSelectorState::GenericDataSelectorState(S2EExecutionState *s, Plugin *p)
{

}

GenericDataSelectorState::~GenericDataSelectorState()
{

}

PluginState *GenericDataSelectorState::clone() const
{
    return new GenericDataSelectorState(*this);
}

PluginState *GenericDataSelectorState::factory(Plugin *p, S2EExecutionState *s)
{
    return new GenericDataSelectorState();
}

bool GenericDataSelectorState::activateRule(const RuleCfg &rule, const ModuleDescriptor &desc)
{
    RuntimeRule rtRule;
    rtRule.pc = desc.ToRuntime(rule.pc);
    rtRule.pid = desc.Pid;
    rtRule.reg = rule.reg;
    rtRule.rule = rule.rule;
    rtRule.concrete = rule.concrete;
    rtRule.value = rule.value;
    rtRule.size = rule.size;
    rtRule.offset = rule.offset;
    rtRule.makeParamCountSymbolic = rule.makeParamCountSymbolic;
    rtRule.makeParamsSymbolic = rule.makeParamsSymbolic;

    m_activeRules.insert(rtRule);

    return true;
}

bool GenericDataSelectorState::deactivateRule(const ModuleDescriptor &desc)
{
    RuntimeRules::iterator it1, it2;

    it1 = m_activeRules.begin();
    while(it1 != m_activeRules.end()) {
        const RuntimeRule &rtr = *it1;
        if (rtr.pc >= desc.LoadBase && rtr.pc < desc.LoadBase + desc.Size) {
            it2 = it1;
            ++it2;
            m_activeRules.erase(*it1);
            it1 = it2;
        }else {
            ++it1;
        }
    }
    return true;
}

bool GenericDataSelectorState::deactivateRule(uint64_t pid)
{
    RuntimeRules::iterator it1, it2;

    it1 = m_activeRules.begin();
    while(it1 != m_activeRules.end()) {
        const RuntimeRule &rtr = *it1;
        if (rtr.pid == pid) {
            it2 = it1;
            ++it2;
            m_activeRules.erase(*it1);
            it1 = it2;
        }else {
            ++it1;
        }
    }
    return true;
}

bool GenericDataSelectorState::check(uint64_t pid, uint64_t pc) const
{
    RuntimeRule rtRule;
    rtRule.pc = pc;
    rtRule.pid = pid;
    return m_activeRules.find(rtRule) != m_activeRules.end();
}

const RuntimeRule &GenericDataSelectorState::getRule(uint64_t pid, uint64_t pc) const
{
    RuntimeRules::const_iterator it;
    RuntimeRule rtRule;
    rtRule.pc = pc;
    rtRule.pid = pid;
    it = m_activeRules.find(rtRule);
    assert(it != m_activeRules.end());
    return *it;
}

}
}
