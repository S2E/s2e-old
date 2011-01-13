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
//#include "cpu.h"
//#include "exec-all.h"
#include "qemu-common.h"
}

#include "BranchCoverage.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/Database.h>

#include <llvm/System/TimeValue.h>

#include <iostream>
#include <sstream>



namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(BranchCoverage, "Branch Coverage plugin", "BranchCoverage",
                  "ExecutionTracer", "ModuleExecutionDetector");

void BranchCoverage::initialize()
{
    std::stringstream ss;
    std::string file;

    m_tracer = (ExecutionTracer*)s2e()->getPlugin("ExecutionTracer");
    assert(m_tracer);


    std::vector<std::string> keyList;
    keyList = s2e()->getConfig()->getListKeys(getConfigKey());
    bool noErrors = true;
    foreach2(it, keyList.begin(), keyList.end()) {
        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << std::endl;
        std::stringstream sk;
        sk << getConfigKey() << "." << *it;
        if (!initSection(sk.str())) {
            noErrors = false;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the branch coverage sections"
            <<std::endl;
        exit(-1);
    }

    //Check that the interceptor is there
    m_executionDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    assert(m_executionDetector);

    //Registering listener
    m_executionDetector->onModuleTranslateBlockEnd.connect(
        sigc::mem_fun(*this, &BranchCoverage::onTranslateBlockEnd)
    );


}

bool BranchCoverage::initSection(const std::string &cfgKey)
{
    //Fetch the coverage type
    std::string covType = s2e()->getConfig()->getString(cfgKey + ".covtype");
    if (covType.compare("aggregated") == 0) {
        return initAggregatedCoverage(cfgKey);
    }else {
        s2e()->getWarningsStream() << "Unsupported type of coverage "
            << covType << std::endl;
        return false;
    }
}

/**
 *  Aggregated means that the coverage for all modules with
 *  the same name will be merged together, no matter what context
 *  the modules are running in.
 */
bool BranchCoverage::initAggregatedCoverage(const std::string &cfgKey)
{
    bool ok;
    std::string key = cfgKey + ".module";
    std::string moduleId =  s2e()->getConfig()->getString(key, "", &ok);

    if (!ok) {
        s2e()->getWarningsStream() << "You must specifiy " << key << std::endl;
        return false;
    }

    //Check that the module id is valid
    if (!m_executionDetector->isModuleConfigured(moduleId)) {
        s2e()->getWarningsStream() << 
            moduleId << " not configured in the execution detector! " << std::endl;
        return false;
    }
    
    m_modules.insert(moduleId);

    return true;
}

void BranchCoverage::onTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor& desc,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    const std::string *moduleId = m_executionDetector->getModuleId(desc);
    if (moduleId == NULL) {
        return;
    }

    if (m_modules.find(*moduleId) == m_modules.end()) {
        return;
    }

    signal->connect(
        sigc::mem_fun(*this, &BranchCoverage::onExecution)
    );
}


void BranchCoverage::onExecution(S2EExecutionState *state, uint64_t pc)
{
    ETranslationBlockType TbType = state->getTb()->s2e_tb_type;

    if (TbType == TB_JMP || TbType == TB_JMP_IND ||
        TbType == TB_COND_JMP || TbType == TB_COND_JMP_IND) {
        ExecutionTraceBranchCoverage e;
        e.pc = pc;
        e.destPc = state->getPc();

        m_tracer->writeData(state, &e, sizeof(ExecutionTraceBranchCoverage), TRACE_BRANCHCOV);
    }
}

} // namespace plugins
} // namespace s2e


