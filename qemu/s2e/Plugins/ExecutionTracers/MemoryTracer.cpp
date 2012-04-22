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

#include <iomanip>

#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include "MemoryTracer.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(MemoryTracer, "Memory tracer plugin", "MemoryTracer", "ExecutionTracer");

MemoryTracer::MemoryTracer(S2E* s2e)
        : Plugin(s2e)
{

}

void MemoryTracer::initialize()
{

    m_tracer = static_cast<ExecutionTracer*>(s2e()->getPlugin("ExecutionTracer"));
    m_execDetector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));

    //Retrict monitoring to configured modules only
    m_monitorModules = s2e()->getConfig()->getBool(getConfigKey() + ".monitorModules");
    if (m_monitorModules && !m_execDetector) {
        s2e()->getWarningsStream() << "MemoryTracer: The monitorModules option requires ModuleExecutionDetector\n";
        exit(-1);
    }

    //Catch all accesses to the stack
    m_monitorStack = s2e()->getConfig()->getBool(getConfigKey() + ".monitorStack");

    //Catch accesses that are above the specified address
    m_catchAbove = s2e()->getConfig()->getInt(getConfigKey() + ".catchAccessesAbove");

    //Whether or not to include host addresses in the trace.
    //This is useful for debugging, bug yields larger traces
    m_traceHostAddresses = s2e()->getConfig()->getBool(getConfigKey() + ".traceHostAddresses");

    //Start monitoring after the specified number of seconds
    bool hasTimeTrigger = false;
    m_timeTrigger = s2e()->getConfig()->getInt(getConfigKey() + ".timeTrigger", 0, &hasTimeTrigger);
    m_elapsedTics = 0;

    bool manualMode = s2e()->getConfig()->getBool(getConfigKey() + ".manualTrigger");

    m_monitorMemory = s2e()->getConfig()->getBool(getConfigKey() + ".monitorMemory");
    m_monitorPageFaults = s2e()->getConfig()->getBool(getConfigKey() + ".monitorPageFaults");
    m_monitorTlbMisses  = s2e()->getConfig()->getBool(getConfigKey() + ".monitorTlbMisses");

    s2e()->getDebugStream() << "MonitorMemory: " << m_monitorMemory << 
    " PageFaults: " << m_monitorPageFaults << " TlbMisses: " << m_monitorTlbMisses << '\n';

    if (hasTimeTrigger) {
        m_timerConnection = s2e()->getCorePlugin()->onTimer.connect(
                sigc::mem_fun(*this, &MemoryTracer::onTimer));
    } else if (manualMode) {
        s2e()->getCorePlugin()->onCustomInstruction.connect(
                sigc::mem_fun(*this, &MemoryTracer::onCustomInstruction));
    } else {
        enableTracing();
    }
}

void MemoryTracer::traceDataMemoryAccess(S2EExecutionState *state,
                               klee::ref<klee::Expr> &address,
                               klee::ref<klee::Expr> &hostAddress,
                               klee::ref<klee::Expr> &value,
                               bool isWrite, bool isIO)
{
    bool isAddrCste = isa<klee::ConstantExpr>(address);
    bool isValCste = isa<klee::ConstantExpr>(value);
    bool isHostAddrCste = isa<klee::ConstantExpr>(hostAddress);

    //Output to the trace entry here
    ExecutionTraceMemory e;
    e.pc = state->getPc();
    e.address = isAddrCste ? cast<klee::ConstantExpr>(address)->getZExtValue(64) : 0xDEADBEEF;
    e.value = isValCste ? cast<klee::ConstantExpr>(value)->getZExtValue(64) : 0xDEADBEEF;
    e.size = klee::Expr::getMinBytesForWidth(value->getWidth());
    e.flags = isWrite*EXECTRACE_MEM_WRITE |
                 isIO*EXECTRACE_MEM_IO;

    e.hostAddress = isHostAddrCste ? cast<klee::ConstantExpr>(hostAddress)->getZExtValue(64) : 0xDEADBEEF;

    if (m_traceHostAddresses) {
        e.flags |= EXECTRACE_MEM_HASHOSTADDR;
    }

    if (!isAddrCste) {
       e.flags |= EXECTRACE_MEM_SYMBADDR;
    }

    if (!isValCste) {
       e.flags |= EXECTRACE_MEM_SYMBVAL;
    }

    if (!isHostAddrCste) {
       e.flags |= EXECTRACE_MEM_SYMBHOSTADDR;
    }

    unsigned strucSize = sizeof(e);
    if (!(e.flags & EXECTRACE_MEM_HASHOSTADDR)) {
       strucSize -= sizeof(e.hostAddress);
    }

    m_tracer->writeData(state, &e, sizeof(e), TRACE_MEMORY);
}

void MemoryTracer::onDataMemoryAccess(S2EExecutionState *state,
                               klee::ref<klee::Expr> address,
                               klee::ref<klee::Expr> hostAddress,
                               klee::ref<klee::Expr> value,
                               bool isWrite, bool isIO)
{
    //XXX: This is a hack.
    //Sometimes the onModuleTransition is not fired properly...
    if (m_execDetector && m_monitorModules && !m_execDetector->getCurrentDescriptor(state)) {
        m_memoryMonitor.disconnect();
        return;
    }

    traceDataMemoryAccess(state, address, hostAddress, value, isWrite, isIO);
}

void MemoryTracer::onModuleTransition(S2EExecutionState *state,
                                       const ModuleDescriptor *prevModule,
                                       const ModuleDescriptor *nextModule)
{
    if(nextModule) {
        m_memoryMonitor =
            s2e()->getCorePlugin()->onDataMemoryAccess.connect(
                sigc::mem_fun(*this, &MemoryTracer::onDataMemoryAccess)
            );
    } else {
        m_memoryMonitor.disconnect();
    }
}


void MemoryTracer::onTlbMiss(S2EExecutionState *state, uint64_t addr, bool is_write)
{
    ExecutionTraceTlbMiss e;
    e.pc = state->getPc();
    e.address = addr;
    e.isWrite = is_write;

    m_tracer->writeData(state, &e, sizeof(e), TRACE_TLBMISS);
}

void MemoryTracer::onPageFault(S2EExecutionState *state, uint64_t addr, bool is_write)
{
    ExecutionTracePageFault e;
    e.pc = state->getPc();
    e.address = addr;
    e.isWrite = is_write;

    m_tracer->writeData(state, &e, sizeof(e), TRACE_PAGEFAULT);
}

void MemoryTracer::enableTracing()
{
    if (m_monitorMemory) {
        s2e()->getMessagesStream() << "MemoryTracer Plugin: Enabling memory tracing" << '\n';
        m_memoryMonitor.disconnect();

        if (m_monitorModules) {
            m_execDetector->onModuleTransition.connect(
                    sigc::mem_fun(*this,
                            &MemoryTracer::onModuleTransition)
                    );
        } else {
            m_memoryMonitor = s2e()->getCorePlugin()->onDataMemoryAccess.connect(
                    sigc::mem_fun(*this, &MemoryTracer::onDataMemoryAccess));
        }
    }

    if (m_monitorPageFaults) {
        s2e()->getMessagesStream() << "MemoryTracer Plugin: Enabling page fault tracing" << '\n';
        m_pageFaultsMonitor.disconnect();
        m_pageFaultsMonitor = s2e()->getCorePlugin()->onPageFault.connect(
                sigc::mem_fun(*this, &MemoryTracer::onPageFault));
    }

    if (m_monitorTlbMisses) {
        s2e()->getMessagesStream() << "MemoryTracer Plugin: Enabling TLB miss tracing" << '\n';
        m_tlbMissesMonitor.disconnect();
        m_tlbMissesMonitor = s2e()->getCorePlugin()->onTlbMiss.connect(
                sigc::mem_fun(*this, &MemoryTracer::onTlbMiss));
    }
}

void MemoryTracer::disableTracing()
{
    m_memoryMonitor.disconnect();
    m_pageFaultsMonitor.disconnect();
    m_tlbMissesMonitor.disconnect();
}

void MemoryTracer::onTimer()
{
    if (m_elapsedTics++ < m_timeTrigger) {
        return;
    }

    enableTracing();

    m_timerConnection.disconnect();
}

void MemoryTracer::onCustomInstruction(S2EExecutionState* state, uint64_t opcode)
{
    if (!OPCODE_CHECK(opcode, MEMORY_TRACER_OPCODE)) {
        return;
    }

    //XXX: remove this mess. Should have a function for extracting
    //info from opcodes.
    opcode >>= 16;
    uint8_t op = opcode & 0xFF;
    opcode >>= 8;


    MemoryTracerOpcodes opc = (MemoryTracerOpcodes)op;
    switch(opc) {
    case Enable:
        enableTracing();
        break;

    case Disable:
        disableTracing();
        break;

    default:
        s2e()->getWarningsStream() << "MemoryTracer: unsupported opcode " << hexval(opc) << '\n';
        break;
    }

}

}
}
