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

#ifndef S2E_PLUGINS_BSOD_H
#define S2E_PLUGINS_BSOD_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include "WindowsMonitor.h"
#include "WindowsImage.h"
#include "WindowsCrashDumpGenerator.h"

namespace s2e {
namespace plugins {


enum BsodCodes
{
    CRITICAL_OBJECT_TERMINATION = 0xf4
};

class BlueScreenInterceptor : public Plugin
{
    S2E_PLUGIN
public:
    BlueScreenInterceptor(S2E* s2e): Plugin(s2e) {}

    void initialize();

    void generateDump(S2EExecutionState *state, const std::string &prefix);
    void generateDumpOnBsod(S2EExecutionState *state, const std::string &prefix);

    void enableCrashDumpGeneration(bool b) {
        m_generateCrashDump = b;
    }

private:
    WindowsMonitor *m_monitor;
    WindowsCrashDumpGenerator *m_crashdumper;
    bool m_generateCrashDump;
    unsigned m_maxDumpCount;
    unsigned m_currentDumpCount;

    void onTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t pc);

    void dumpCriticalObjectTermination(S2EExecutionState *state);
    void dispatchErrorCodes(S2EExecutionState *state);

    void onBsod(S2EExecutionState *state, uint64_t pc);

 };




} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
