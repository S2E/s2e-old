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

    m_Rules.push_back(cfg);
    return true;
}


void GenericDataSelector::onTranslateBlockStart(ExecutionSignal* signal, S2EExecutionState *state, 
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
}
    
void GenericDataSelector::onExecution(S2EExecutionState *state, uint64_t pc,
                                 unsigned ruleIdx)
{
    const RuleCfg &r = m_Rules[ruleIdx];

    if (r.rule == "rsagenkey") {
        injectRsaGenKey(state);
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
    klee::ref<klee::Expr> newKeySize = getUpperBound(2048, klee::Expr::Int32);
    klee::ref<klee::Expr> newExponent = getOddValue(klee::Expr::Int32, 65537);

    s2e()->getMessagesStream() << "newKeySize=" << newKeySize << std::endl;
    s2e()->getMessagesStream() << "newExponent=" << newExponent << std::endl;

    state->writeMemory(state->getSp()+sizeof(uint32_t), newKeySize);
    state->writeMemory(state->getSp()+2*sizeof(uint32_t), newExponent);
}
}
}