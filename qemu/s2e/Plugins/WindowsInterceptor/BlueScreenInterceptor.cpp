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

#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/S2EExecutor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsApi/Api.h>

#include "BlueScreenInterceptor.h"

#include <iomanip>
#include <sstream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(BlueScreenInterceptor, "Intercepts Windows blue screens of death and generated bug reports",
                  "BlueScreenInterceptor", "WindowsMonitor");

void BlueScreenInterceptor::initialize()
{
    m_monitor = static_cast<WindowsMonitor*>(s2e()->getPlugin("WindowsMonitor"));
    assert(m_monitor);

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
            sigc::mem_fun(*this,
                    &BlueScreenInterceptor::onTranslateBlockStart));

    bool ok;
    m_generateCrashDump = s2e()->getConfig()->getBool(getConfigKey() + ".generateCrashDump", false, &ok);

    m_crashdumper = static_cast<WindowsCrashDumpGenerator*>(s2e()->getPlugin("WindowsCrashDumpGenerator"));

    if (m_generateCrashDump) {
        if (!m_crashdumper) {
            s2e()->getWarningsStream() << "The WindowsCrashDumpGenerator plugin is required with the " <<
                    "generateCrashDump option." << '\n';
            exit(-1);
        }
    }

    //How many dumps to generate at most ?
    //Dumps are large and there can be many of them.
    m_currentDumpCount = 0;
    m_maxDumpCount = s2e()->getConfig()->getInt(getConfigKey() + ".maxDumpCount", (int64_t)-1, &ok);
    s2e()->getDebugStream() << "BlueScreenInterceptor: Maximum number of dumps:" << m_maxDumpCount << '\n';
}

void BlueScreenInterceptor::onTranslateBlockStart(
    ExecutionSignal *signal,
    S2EExecutionState *state,
    TranslationBlock *tb,
    uint64_t pc)
{
    if (!m_monitor->CheckPanic(pc)) {
        return;
    }

    signal->connect(sigc::mem_fun(*this,
        &BlueScreenInterceptor::onBsod));
}

void BlueScreenInterceptor::onBsod(
        S2EExecutionState *state, uint64_t pc)
{
    llvm::raw_ostream &os = s2e()->getWarningsStream(state);

    os << "Killing state "  << state->getID() << " because of BSOD " << '\n';

    dispatchErrorCodes(state);

#if 0
    ModuleExecutionDetector *exec = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    if (exec) {
        exec->dumpMemory(state, os, state->getSp(), 512);
    }else {
        state->dumpStack(512);
    }
#endif

    if (m_generateCrashDump && m_currentDumpCount < m_maxDumpCount) {
        ++m_currentDumpCount;
        //XXX: Will have to fix this
        if (m_crashdumper) {
            m_crashdumper->generateDumpOnBsod(state, "bsod");
        }
    }

    s2e()->getExecutor()->terminateStateEarly(*state, "Killing because of BSOD");
}

void BlueScreenInterceptor::dumpCriticalObjectTermination(S2EExecutionState *state)
{
    uint32_t terminatingObjectType;
    uint32_t terminatingObject;
    uint32_t processImageName;
    uint32_t message;

    bool ok = true;
    ok &= WindowsApi::readConcreteParameter(state, 1, &terminatingObjectType);
    ok &= WindowsApi::readConcreteParameter(state, 2, &terminatingObject);
    ok &= WindowsApi::readConcreteParameter(state, 3, &processImageName);
    ok &= WindowsApi::readConcreteParameter(state, 4, &message);

    if (!ok) {
        s2e()->getDebugStream() << "Could not read BSOD parameters" << '\n';
    }

    std::string strMessage, strImage;
    ok &= state->readString(message, strMessage, 256);
    ok &= state->readString(processImageName, strImage, 256);

    s2e()->getDebugStream(state) <<
            "CRITICAL_OBJECT_TERMINATION" << '\n' <<
            "ImageName: " << strImage << '\n' <<
            "Message:   " << strMessage << '\n';
}

void BlueScreenInterceptor::dispatchErrorCodes(S2EExecutionState *state)
{
    uint32_t errorCode;

    WindowsApi::readConcreteParameter(state, 0, &errorCode);
    switch(errorCode) {
    case CRITICAL_OBJECT_TERMINATION:
        dumpCriticalObjectTermination(state);
        break;

    default:
        s2e()->getDebugStream(state) << "Unknown BSOD code " << errorCode << '\n';
        break;
    }
}


}
}
