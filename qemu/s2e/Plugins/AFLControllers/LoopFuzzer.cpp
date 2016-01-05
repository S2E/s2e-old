/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2015, Information Security Laboratory, NUDT
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
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#include "LoopFuzzer.h"
extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <sysemu.h>
#include <sys/shm.h>
}
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/Plugin.h>

#include <iomanip>
#include <cctype>

#include <algorithm>
#include <fstream>
#include <vector>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>    /**/
#include <errno.h>     /*errno*/
#include <unistd.h>    /*ssize_t*/
#include <sys/types.h>
#include <sys/stat.h>  /*mode_t*/
#include <stdlib.h>

using namespace llvm::sys;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(LoopFuzzer, "LoopFuzzer plugin", "LoopFuzzer",
        "ModuleExecutionDetector", "HostFiles", "ForkController");

LoopFuzzer::~LoopFuzzer()
{
}

void LoopFuzzer::initialize()
{
    FuzzySearcher::initialize();
    m_forkcontroller = static_cast<ForkController*>(s2e()->getPlugin(
            "ForkController"));
    if (!m_forkcontroller) {
        std::cerr << "Could not find ForkController plug-in. " << '\n';
        exit(0);
    }
    m_forkcontroller->onStuckinSymLoop.connect(
    sigc::mem_fun(*this, &LoopFuzzer::onStuckinSymLoop));
    m_forkcontroller->onGetoutfromSymLoop.connect(
    sigc::mem_fun(*this, &LoopFuzzer::onGetoutfromSymLoop));
}
void LoopFuzzer::onStateSwitchEnd(S2EExecutionState *currentState,
        S2EExecutionState *nextState)
{
    if (currentState && (currentState->m_silently_concretized)) {
        //s2e()->getExecutor()->terminateStateEarly(*currentState, "FuzzySearcher: terminate silently concretized state");
    }
    // if S2E only has the carry on state, then carry on to next iteration
    if (nextState && nextState->m_is_carry_on_state) {
        if (m_verbose) {
            s2e()->getDebugStream()
                    << "LoopFuzzer: We only have the seed state, now fetching new testcase.\n";
        }
        if (!m_AFLStarted) { //if AFL not started, then starts it
            startAFL();
            m_AFLStarted = true;
            sleep(10); //let us wait for new testcases
        }
        assert(m_AFLStarted && "AFL has not started, why?");
        {
            std::stringstream ssAflQueue;
            ssAflQueue << m_aflOutputpool << "queue/";
            Path inicasepool_llvm(m_inicasepool);
            Path aflqueuecasepool_llvm(ssAflQueue.str());
            ClearbeforeCopyto(inicasepool_llvm, aflqueuecasepool_llvm);
        }
        prepareNextState(nextState);
    }
}
/*
 * If we are stuck in a symbolic value controlled loop, because m_forkcontroller has helped us to
 * control the fork, so here we just generate the testcase for AFL.
 */
void LoopFuzzer::onStuckinSymLoop(S2EExecutionState* state, uint64_t pc)
{
    //generate the testcase for AFL
    //step1. construct name and generate the testcase
    std::stringstream ssgeneratedCase;
    std::string destfilename;
    if (1) {
        ssgeneratedCase << "testcases/" << getCurrentLoop() << "-"
                << state->getID() << "-" << hexval(pc) << "-"
                << m_symbolicfilename; // Lopp-StateID-pc-m_symbolicfilename
        destfilename = s2e()->getOutputFilename(ssgeneratedCase.str().c_str());
    } else {
        ssgeneratedCase << m_aflOutputpool << "queue/" << getCurrentLoop()
                << "-" << state->getID() << "-" << m_symbolicfilename; // Lopp-StateID-m_symbolicfilename
        destfilename = ssgeneratedCase.str();
    }
    if (m_verbose) {
        s2e()->getDebugStream() << "LoopFuzzer: Generating testcase: "
                << destfilename << ".\n";
    }
    DECLARE_PLUGINSTATE(LoopFuzzerState, state);
    if (!plgState->m_hasgeneratedtestcase) {
        plgState->m_hasgeneratedtestcase = generateCaseFile(state,
                destfilename);
    }
}

void LoopFuzzer::onGetoutfromSymLoop(S2EExecutionState* state, uint64_t pc)
{
    if (m_verbose) {
        s2e()->getDebugStream()
                << "LoopFuzzer: we are getting from a loop and starting to execute BB: "
                << hexval(pc) << ".\n";
    }
    DECLARE_PLUGINSTATE(LoopFuzzerState, state);
    if (plgState->m_hasgeneratedtestcase)
        plgState->m_hasgeneratedtestcase = false;
}

void LoopFuzzer::startAFL(void)
{
    std::stringstream aflCmdline;
    std::string generated_dir = s2e()->getOutputDirectory() + "/testcases";
    aflCmdline << m_aflRoot << "afl-fuzz -m 4096M -t 5000 -i " << generated_dir
            << " -o " << m_aflOutputpool << (m_aflBinaryMode ? " -Q " : "")
            << m_aflAppArgs << " &";
    if (m_verbose) {
        s2e()->getDebugStream() << "LoopFuzzer: AFL command line is: "
                << aflCmdline.str() << "\n";
    }
    system(aflCmdline.str().c_str()); //we don't want to suspend it, so we add "&":
}

LoopFuzzerState::LoopFuzzerState()
{
    m_isinLoop = false;
    m_hasgeneratedtestcase = false;
}

LoopFuzzerState::LoopFuzzerState(S2EExecutionState *s, Plugin *p)
{
    m_plugin = static_cast<LoopFuzzer*>(p);
    m_state = s;
    m_prev_loc = 0;
    m_isinLoop = false;
    m_hasgeneratedtestcase = false;
}

LoopFuzzerState::~LoopFuzzerState()
{
}

PluginState *LoopFuzzerState::clone() const
{
    return new LoopFuzzerState(*this);
}

PluginState *LoopFuzzerState::factory(Plugin *p, S2EExecutionState *s)
{
    LoopFuzzerState *ret = new LoopFuzzerState(s, p);
    return ret;
}
}
} /* namespace s2e */
