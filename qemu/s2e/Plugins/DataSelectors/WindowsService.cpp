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

#include "WindowsService.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>


#include <iostream>
#include <sstream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(WindowsService, "Selecting symbolic data for Windows services", 
                  "WindowsService", "WindowsMonitor", "ModuleExecutionDetector" );

//WindowsService-specific initialization here
void WindowsService::initialize()
{
    m_WindowsMonitor = (WindowsMonitor*)s2e()->getPlugin("WindowsMonitor");
    assert(m_WindowsMonitor);

    m_executionDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    assert(m_executionDetector);

    //Read the cfg file and call init sections
    DataSelector::initialize();
}

bool WindowsService::initSection(const std::string &cfgKey, const std::string &svcId)
{
    bool ok;
    std::string moduleId = s2e()->getConfig()->getString(cfgKey + ".module", "", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".module\n";
        return false;
    }
    
    if (!m_ExecDetector->isModuleConfigured(moduleId)) {
        s2e()->getWarningsStream() << moduleId << " is not configured in the execution detector!\n";
        return false;
    }

    m_ServiceCfg.serviceId = svcId;
    m_ServiceCfg.moduleId = moduleId;
    m_ServiceCfg.makeParamsSymbolic = s2e()->getConfig()->getBool(cfgKey + ".makeParamsSymbolic");
    m_ServiceCfg.makeParamCountSymbolic = s2e()->getConfig()->getBool(cfgKey + ".makeParamCountSymbolic");

    m_modules.insert(moduleId);

    //Registering listener
    m_TbConnection = m_ExecDetector->onModuleTranslateBlockStart.connect(
        sigc::mem_fun(*this, &WindowsService::onTranslateBlockStart)
    );

    return true;
}

void WindowsService::onTranslateBlockStart(ExecutionSignal *signal, 
                                      S2EExecutionState *state,
                                      const ModuleDescriptor &desc,
                                      TranslationBlock *tb,
                                      uint64_t pc)
{
    Exports E;
    Exports::iterator eit;

    const std::string *moduleId = m_executionDetector->getModuleId(desc);
    if (moduleId == NULL) {
        return;
    }

    if (m_modules.find(*moduleId) == m_modules.end()) {
        return;
    }

    if (!m_WindowsMonitor->getExports(state, desc, E)) {
        s2e()->getWarningsStream() << 
            "Could not get exports for module " << *moduleId << '\n';
        return;
    }

    eit = E.find("ServiceMain");
    if (eit == E.end()) {
        s2e()->getMessagesStream() << 
            "Could not find the ServiceMain entry point for " << *moduleId << '\n';
        m_TbConnection.disconnect();
        return;
    }

    s2e()->getWarningsStream() << "Found ServiceMain at " << hexval((*eit).second) << '\n';

    //XXX: cache the export table for reuse.
    if (pc != (*eit).second) {
        s2e()->getMessagesStream() << 
            "ServiceMain " << hexval(pc) << " " << hexval((*eit).second) << '\n';
        return;
    }

    s2e()->getMessagesStream() << 
            "Found ServiceMain for "<< *moduleId << " "  << hexval(pc)
            << " " << hexval((*eit).second) <<'\n';

    
    signal->connect(
        sigc::mem_fun(*this, &WindowsService::onExecution)
    );

    //Must handle multiple services for generality.
    m_TbConnection.disconnect();


}

void WindowsService::onExecution(S2EExecutionState *state, uint64_t pc)
{
    //Parse the arguments here
    uint32_t paramCount;
    uint32_t paramsArray;
    
    s2e()->getMessagesStream() << "WindowsService entered\n";
    //XXX: hard-coded pointer size assumptions
    SREAD(state, state->getSp()+sizeof(uint32_t), paramCount);
    SREAD(state, state->getSp()+2*sizeof(uint32_t), paramsArray);
    
    s2e()->getMessagesStream() << "WindowsService paramCount="  <<
        paramCount << " - " << hexval(paramsArray) << "esp=" << hexval(state->getSp())  << '\n';

    for(unsigned i=0; i<paramCount; i++) {
        uint32_t paramPtr;
        std::string param;
        SREAD(state, paramsArray+i*sizeof(uint32_t), paramPtr);
        if (!state->readUnicodeString(paramPtr, param)) {
            continue;
        }
        s2e()->getMessagesStream() << "WindowsService param" << i << " - " <<
            param << '\n';

        if (m_ServiceCfg.makeParamsSymbolic) {
            makeUnicodeStringSymbolic(state, paramPtr);
        }
    }

    //Make number of params symbolic
    if (m_ServiceCfg.makeParamCountSymbolic) {
        klee::ref<klee::Expr> v = getUpperBound(state, paramCount, klee::Expr::Int32);
        s2e()->getMessagesStream() << "ParamCount is now " << v << '\n';
        state->writeMemory(state->getSp()+sizeof(uint32_t), v);
    }
}

} // namespace plugins
} // namespace s2e
