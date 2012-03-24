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

#ifndef S2E_PLUGINS_MEMTRACER_H
#define S2E_PLUGINS_MEMTRACER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/Opcodes.h>
#include <string>
#include "ExecutionTracer.h"
#include <s2e/Plugins/ModuleExecutionDetector.h>

namespace s2e{
namespace plugins{


/** Handler required for KLEE interpreter */
class MemoryTracer : public Plugin
{
    S2E_PLUGIN

private:

public:
    MemoryTracer(S2E* s2e);

    void initialize();

    enum MemoryTracerOpcodes {
        Enable = 0,
        Disable = 1
    };

private:
    bool m_monitorPageFaults;
    bool m_monitorTlbMisses;
    bool m_monitorMemory;

    bool m_monitorModules;
    bool m_monitorStack;
    bool m_traceHostAddresses;
    uint64_t m_catchAbove;

    uint64_t m_timeTrigger;
    uint64_t m_elapsedTics;
    sigc::connection m_timerConnection;

    sigc::connection m_memoryMonitor;
    sigc::connection m_pageFaultsMonitor;
    sigc::connection m_tlbMissesMonitor;

    ExecutionTracer *m_tracer;
    ModuleExecutionDetector *m_execDetector;

    void onTlbMiss(S2EExecutionState *state, uint64_t addr, bool is_write);
    void onPageFault(S2EExecutionState *state, uint64_t addr, bool is_write);

    void onTimer();

    void enableTracing();
    void disableTracing();
    void onCustomInstruction(S2EExecutionState* state, uint64_t opcode);

    void onDataMemoryAccess(S2EExecutionState *state,
                                   klee::ref<klee::Expr> address,
                                   klee::ref<klee::Expr> hostAddress,
                                   klee::ref<klee::Expr> value,
                                   bool isWrite, bool isIO);

    void onModuleTransition(S2EExecutionState *state,
                            const ModuleDescriptor *prevModule,
                            const ModuleDescriptor *nextModule);
public:
    //May be called directly by other plugins
    void traceDataMemoryAccess(S2EExecutionState *state,
                                   klee::ref<klee::Expr> &address,
                                   klee::ref<klee::Expr> &hostAddress,
                                   klee::ref<klee::Expr> &value,
                                   bool isWrite, bool isIO);
};


}
}

#endif
