#include "WindowsService.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>


#include <iostream>
#include <sstream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(WindowsService, "Selecting symbolic data for Windows services", 
                  "WindowsService", "WindowsMonitor", );

//WindowsService-specific initialization here
void WindowsService::initialize()
{
    m_WindowsMonitor = (WindowsMonitor*)s2e()->getPlugin("WindowsMonitor");
    assert(m_WindowsMonitor);

    //Read the cfg file and call init sections
    DataSelector::initialize();
}

bool WindowsService::initSection(const std::string &cfgKey, const std::string &svcId)
{
    bool ok;
    std::string moduleId = s2e()->getConfig()->getString(cfgKey + ".module", "", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".module" << std::endl;
        return false;
    }
    
    if (!m_ExecDetector->isModuleConfigured(moduleId)) {
        s2e()->getWarningsStream() << moduleId << " is not configured in the execution detector!" << std::endl;   
        return false;
    }

    m_ServiceCfg.serviceId = svcId;
    m_ServiceCfg.moduleId = moduleId;
    m_ServiceCfg.makeParamsSymbolic = s2e()->getConfig()->getBool(cfgKey + ".makeParamsSymbolic");
    m_ServiceCfg.makeParamCountSymbolic = s2e()->getConfig()->getBool(cfgKey + ".makeParamCountSymbolic");

    m_Modules.insert(moduleId);

    //Registering listener
    m_TbConnection = m_ExecDetector->onModuleTranslateBlockStart.connect(
        sigc::mem_fun(*this, &WindowsService::onTranslateBlockStart)
    );

    return true;
}

void WindowsService::onTranslateBlockStart(ExecutionSignal *signal, 
                                      S2EExecutionState *state,
                                      const ModuleExecutionDesc*desc,
                                      TranslationBlock *tb,
                                      uint64_t pc)
{
    Exports E;
    Exports::iterator eit;

    if (m_Modules.find(desc->id) == m_Modules.end()) {
        return;
    }

    if (!m_WindowsMonitor->getExports(state, desc->descriptor, E)) {
        s2e()->getWarningsStream() << 
            "Could not get exports for module " << desc->id << std::endl;   
        return;
    }

    eit = E.find("ServiceMain");
    if (eit == E.end()) {
        s2e()->getMessagesStream() << 
            "Could not find the ServiceMain entry point for " << desc->id << std::endl;   
        m_TbConnection.disconnect();
        return;
    }

    s2e()->getWarningsStream() << "Found ServiceMain at 0x" << std::hex << (*eit).second << std::endl;

    //XXX: cache the export table for reuse.
    if (pc != (*eit).second) {
        s2e()->getMessagesStream() << 
            std::hex << "ServiceMain " << pc << " " << (*eit).second << std::endl;
        return;
    }

    s2e()->getMessagesStream() << 
            std::hex << "Found ServiceMain for "<< desc->id << " "  << pc << " " << (*eit).second << std::endl;

    
    signal->connect(
        sigc::bind(sigc::mem_fun(*this, &WindowsService::onExecution),
                   (const ModuleExecutionDesc*) desc)
    );

    //Must handle multiple services for generality.
    m_TbConnection.disconnect();


}

void WindowsService::onExecution(S2EExecutionState *state, uint64_t pc,
                                 const ModuleExecutionDesc* desc)
{
    //Parse the arguments here
    uint32_t paramCount;
    uint32_t paramsArray;
    
    s2e()->getMessagesStream() << "WindowsService entered" << std::endl;
    //XXX: hard-coded pointer size assumptions
    SREAD(state, state->getSp()+sizeof(uint32_t), paramCount);
    SREAD(state, state->getSp()+2*sizeof(uint32_t), paramsArray);
    
    s2e()->getMessagesStream() << "WindowsService paramCount="  <<
        paramCount << " - " << std::hex << paramsArray << "esp=" << state->getSp() <<std::endl;

    for(unsigned i=0; i<paramCount; i++) {
        uint32_t paramPtr;
        std::string param;
        SREAD(state, paramsArray+i*sizeof(uint32_t), paramPtr);
        if (!state->readUnicodeString(paramPtr, param)) {
            continue;
        }
        s2e()->getMessagesStream() << "WindowsService param" << i << " - " <<
            param << std::endl;

        if (m_ServiceCfg.makeParamsSymbolic) {
            makeUnicodeStringSymbolic(state, paramPtr);
        }
    }

    //Make number of params symbolic
    if (m_ServiceCfg.makeParamCountSymbolic) {
        klee::ref<klee::Expr> v = getUpperBound(paramCount, klee::Expr::Int32);
        s2e()->getMessagesStream() << "ParamCount is now " << v << std::endl; 
        state->writeMemory(state->getSp()+sizeof(uint32_t), v);
    }
}

} // namespace plugins
} // namespace s2e
