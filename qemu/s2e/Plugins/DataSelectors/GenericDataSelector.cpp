extern "C" {
#include "config.h"
#include "qemu-common.h"
}


#include "GenericDataSelector.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>


#include <iostream>
#include <sstream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(GenericDataSelector, "Generic symbolic injection framework", 
                  "GenericDataSelector", "Interceptor", );

//WindowsService-specific initialization here
void GenericDataSelector::initialize()
{
    m_Monitor = (OSMonitor*)s2e()->getPlugin("Interceptor");
    assert(m_Monitor);

    //Read the cfg file and call init sections
    DataSelector::initialize();

    //Registering listener
    m_TbConnection = m_ExecDetector->onModuleTranslateBlockStart.connect(
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
    cfg.moduleId = s2e()->getConfig()->getString(cfgKey + ".module", "", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".module" << std::endl;
        return false;
    }

    cfg.rule = s2e()->getConfig()->getString(cfgKey + ".rule", "", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".rule" << std::endl;
        return false;
    }

    cfg.pc = s2e()->getConfig()->getInt(cfgKey + ".fcn", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".function" << std::endl;
        return false;
    }

    cfg.reg = s2e()->getConfig()->getInt(cfgKey + ".register", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You might have forgotten to specify " << cfgKey <<  ".register" << std::endl;
    }

    cfg.makeParamsSymbolic = s2e()->getConfig()->getBool(cfgKey + ".makeParamsSymbolic");
    cfg.makeParamCountSymbolic = s2e()->getConfig()->getBool(cfgKey + ".makeParamCountSymbolic");


    m_Rules.push_back(cfg);
    return true;
}


void GenericDataSelector::onTranslateBlockStart(ExecutionSignal* signal, S2EExecutionState *state, 
        const ModuleExecutionDesc*desc,
        TranslationBlock *tb, uint64_t pc)
{
    activateRule(signal, state, desc, tb, pc);

}

void GenericDataSelector::onTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleExecutionDesc*desc,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    activateRule(signal, state, desc, tb, endPc);
}

 
void GenericDataSelector::activateRule(
                                      ExecutionSignal* signal,
                                      S2EExecutionState *state,
                                      const ModuleExecutionDesc*desc,
                                       TranslationBlock *tb, uint64_t pc)
{
    unsigned idx=0;
    foreach2(it, m_Rules.begin(), m_Rules.end()) {
        const RuleCfg &r = *it;
        if (r.moduleId == desc->id && r.pc == desc->descriptor.ToNativeBase(pc)) {
            signal->connect(
                    sigc::bind(sigc::mem_fun(*this, &GenericDataSelector::onExecution),
                    (unsigned)idx)
                    );
        }
        idx++;
    }
    return;
}

void GenericDataSelector::onExecution(S2EExecutionState *state, uint64_t pc,
                                 unsigned ruleIdx)
{
    const RuleCfg &r = m_Rules[ruleIdx];

    if (r.rule == "rsagenkey") {
        injectRsaGenKey(state);
    }else if  (r.rule == "injectreg") {
        injectRegister(state, r.reg);
    }else if  (r.rule == "injectmain") {
        injectMainArgs(state, &r);
    }
}

void GenericDataSelector::injectMainArgs(S2EExecutionState *state, const RuleCfg *rule)
{
    //Parse the arguments here
    uint32_t paramCount;
    uint32_t paramsArray;
    
    s2e()->getDebugStream() << "Injecting main() arguments" << std::endl;
    //XXX: hard-coded pointer size assumptions
    SREAD(state, state->getSp()+sizeof(uint32_t), paramCount);
    SREAD(state, state->getSp()+2*sizeof(uint32_t), paramsArray);
    
    s2e()->getMessagesStream() << "main paramCount="  <<
        paramCount << " - " << std::hex << paramsArray << "esp=" << state->getSp() <<std::endl;

    for(unsigned i=0; i<paramCount; i++) {
        uint32_t paramPtr;
        std::string param;
        SREAD(state, paramsArray+i*sizeof(uint32_t), paramPtr);
        if (!state->readString(paramPtr, param)) {
            continue;
        }
        s2e()->getMessagesStream() << "main param" << i << " - " <<
            param << std::endl;

        if (rule->makeParamsSymbolic) {
            makeStringSymbolic(state, paramPtr);
        }
    }

    //Make number of params symbolic
    if (rule->makeParamCountSymbolic) {
        klee::ref<klee::Expr> v = getUpperBound(state, paramCount, klee::Expr::Int32);
        s2e()->getMessagesStream() << "ParamCount is now " << v << std::endl; 
        state->writeMemory(state->getSp()+sizeof(uint32_t), v);
    }
}

void GenericDataSelector::injectRegister(S2EExecutionState *state, unsigned reg)
{
    s2e()->getDebugStream() << "Injecting symbolic value in register " << reg << std::endl;
    klee::ref<klee::Expr> symb = state->createSymbolicValue(klee::Expr::Int32);
    if (reg < 8) {
        state->writeCpuRegister(CPU_OFFSET(regs) + reg * sizeof(target_ulong), symb);
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

    s2e()->getMessagesStream() << "Caught RSA_generate_key" << std::endl <<
        "origKeySize=" << origKeySize << " origExponent=" << origExponent <<
        " origCallBack=" << std::hex << origCallBack << std::endl;

    //Now we replace the arguments with properly constrained symbolic values
    klee::ref<klee::Expr> newKeySize = getUpperBound(state, 2048, klee::Expr::Int32);
    klee::ref<klee::Expr> newExponent = getOddValue(state, klee::Expr::Int32, 65537);

    s2e()->getMessagesStream() << "newKeySize=" << newKeySize << std::endl;
    s2e()->getMessagesStream() << "newExponent=" << newExponent << std::endl;

    state->writeMemory(state->getSp()+sizeof(uint32_t), newKeySize);
    state->writeMemory(state->getSp()+2*sizeof(uint32_t), newExponent);
}
}
}
