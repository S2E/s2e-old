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

#ifndef S2E_PLUGINS_EXECTRACER_H
#define S2E_PLUGINS_EXECTRACER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>
#include <s2e/S2EExecutionState.h>

#include <stdio.h>

#include "TraceEntries.h"

namespace s2e {
namespace plugins {

//Maps a module descriptor to an id, for compression purposes
typedef std::multimap<ModuleDescriptor, uint16_t, ModuleDescriptor::ModuleByLoadBase> ExecTracerModules;

/**
 *  This plugin manages the binary execution trace file.
 *  It makes sure that all the writes properly go through it.
 *  Each write is encapsulated in an ExecutionTraceItem before being
 *  written to the file.
 */
class ExecutionTracer : public Plugin
{
    S2E_PLUGIN

    std::string m_fileName;
    FILE* m_LogFile;
    uint32_t m_CurrentIndex;
    OSMonitor *m_Monitor;
    ExecTracerModules m_Modules;

    uint16_t getCompressedId(const ModuleDescriptor *desc);

    void onTimer();
    void createNewTraceFile(bool append);
public:
    ExecutionTracer(S2E* s2e): Plugin(s2e) {}
    ~ExecutionTracer();
    void initialize();

    uint32_t writeData(
            const S2EExecutionState *state,
            void *data, unsigned size, ExecTraceEntryType type);

    void flush();
private:

    void onFork(S2EExecutionState *state,
                const std::vector<S2EExecutionState*>& newStates,
                const std::vector<klee::ref<klee::Expr> >& newConditions
                );

    void onProcessFork(bool preFork, bool isChild, unsigned parentProcId);


};

#if 0
class ExecutionTracerState: public PluginState
{
private:
    unsigned m_previousItemIndex;

public:
    ExecutionTracerState();
    virtual ~ExecutionTracerState();
    virtual ExecutionTracerState* clone() const;
    static PluginState *factory();

    friend class ExecutionTracer;
};
#endif

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
