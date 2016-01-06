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
 *     * Neither the name of the Information Security Laboratory, NUDT nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INFORMATION SECURITY LABORATORY, NUDT BE LIABLE
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

#ifndef FORKCONTROLLER_H_

#define FORKCONTROLLER_H_

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Signals/Signals.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>
namespace s2e {
namespace plugins {

class ForkController;

class ForkControllerState: public PluginState
{
private:
    bool m_inloop; // whether we are in a configured loop

public:
    ForkControllerState()
    {
        m_inloop = false;
    }

    ~ForkControllerState()
    {
    }

    static PluginState *factory(Plugin*, S2EExecutionState*)
    {
        return new ForkControllerState();
    }

    ForkControllerState *clone() const
    {
        return new ForkControllerState(*this);
    }
    friend class ForkController;
};

class ForkController: public Plugin
{
S2E_PLUGIN

    typedef std::set<uint64_t> LoopBody;
    typedef std::set<LoopBody> Loops;
    ModuleExecutionDetector *m_detector;
    RangeEntries m_forkRanges;
    std::string m_mainModule;   // main module name (i.e. target binary)
    std::string m_loopfilename; // loop file name

    bool m_controlloop;
    Loops m_loops;
public:
    virtual ~ForkController();
    ForkController(S2E* s2e) :
            Plugin(s2e)
    {
        m_detector = NULL;
        m_controlloop = false;
    }
    void initialize();

public:
    void slotExecuteBlockStart(S2EExecutionState* state, uint64_t pc);
    void slotExecuteBlockEnd(S2EExecutionState* state, uint64_t pc);

    void onModuleTranslateBlockStart(ExecutionSignal*, S2EExecutionState*,
            const ModuleDescriptor &, TranslationBlock*, uint64_t);
    void onModuleTranslateBlockEnd(ExecutionSignal *signal,
            S2EExecutionState* state, const ModuleDescriptor &module,
            TranslationBlock *tb, uint64_t endPc, bool staticTarget,
            uint64_t targetPc);

    // Triggered when we fall into a symbolically executed loop
    sigc::signal<void, S2EExecutionState*, /* currentState */
    uint64_t> /* pc */
    onStuckinSymLoop;
    // Triggered when we get out from a symbolically executed loop
    sigc::signal<void, S2EExecutionState*, /* currentState */
    uint64_t> /* pc */
    onGetoutfromSymLoop;

    bool getLoopsfromFile(void);
};

} // namespace plugins
} // namespace s2e

#endif /* FORKCONTROLLER_H_ */
