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
#include "qemu-common.h"
}

#include "FunctionMonitor.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(FunctionMonitor, "Function calls/returns monitoring plugin", "",);

void FunctionMonitor::initialize()
{
    s2e()->getCorePlugin()->onTranslateBlockEnd.connect(
            sigc::mem_fun(*this, &FunctionMonitor::slotTranslateBlockEnd));

    s2e()->getCorePlugin()->onTranslateJumpStart.connect(
            sigc::mem_fun(*this, &FunctionMonitor::slotTranslateJumpStart));

#if 0
    //Cannot do this here, because we do not have an execution state at this point.
    if(s2e()->getConfig()->getBool(getConfigKey() + ".enableTracing")) {
        getCallSignal(0, 0)->connect(sigc::mem_fun(*this,
                                     &FunctionMonitor::slotTraceCall));
    }
#endif

    m_monitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));
}

//XXX: Implement onmoduleunload to automatically clear all call signals
FunctionMonitor::CallSignal* FunctionMonitor::getCallSignal(
        S2EExecutionState *state,
        uint64_t pc, uint64_t pid)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->getCallSignal(pc, pid);
}

void FunctionMonitor::slotTranslateBlockEnd(ExecutionSignal *signal,
                                      S2EExecutionState *state,
                                      TranslationBlock *tb,
                                      uint64_t inspc, bool staticTarget, uint64_t targetPc)
{
    /* We intercept all call and ret translation blocks */
    if (tb->s2e_tb_type == TB_CALL || tb->s2e_tb_type == TB_CALL_IND) {
        signal->connect(sigc::mem_fun(*this,
                            &FunctionMonitor::slotCall));
    }
}

void FunctionMonitor::slotTranslateJumpStart(ExecutionSignal *signal,
                                             S2EExecutionState *state,
                                             TranslationBlock *,
                                             uint64_t, int jump_type)
{
    if(jump_type == JT_RET || jump_type == JT_LRET) {
        signal->connect(sigc::mem_fun(*this,
                            &FunctionMonitor::slotRet));
    }
}

void FunctionMonitor::slotCall(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->slotCall(state, pc);
}

//See notes for slotRet to see how to use this function.
void FunctionMonitor::eraseSp(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->slotRet(state, pc, false);
}



void FunctionMonitor::slotRet(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->slotRet(state, pc, true);
}


void FunctionMonitor::slotTraceCall(S2EExecutionState *state, FunctionMonitorState *fns)
{
    static int f = 0;

    FunctionMonitor::ReturnSignal returnSignal;
    returnSignal.connect(sigc::bind(sigc::mem_fun(*this, &FunctionMonitor::slotTraceRet), f));
    fns->registerReturnSignal(state, returnSignal);

    s2e()->getMessagesStream(state) << "Calling function " << f
                << " at " << hexval(state->getPc()) << std::endl;
    ++f;
}

void FunctionMonitor::slotTraceRet(S2EExecutionState *state, int f)
{
    s2e()->getMessagesStream(state) << "Returning from function "
                << f << std::endl;
}


FunctionMonitorState::FunctionMonitorState()
{

}

FunctionMonitorState::~FunctionMonitorState()
{

}

FunctionMonitorState* FunctionMonitorState::clone() const
{
    FunctionMonitorState *ret = new FunctionMonitorState(*this);
    m_plugin->s2e()->getDebugStream() << "Forking FunctionMonitorState ret=" << std::hex << ret << std::endl;
    assert(ret->m_returnDescriptors.size() == m_returnDescriptors.size());
    return ret;
}

PluginState *FunctionMonitorState::factory(Plugin *p, S2EExecutionState *s)
{
    FunctionMonitorState *ret = new FunctionMonitorState();
    ret->m_plugin = static_cast<FunctionMonitor*>(p);
    return ret;
}

FunctionMonitor::CallSignal* FunctionMonitorState::getCallSignal(
        uint64_t pc, uint64_t pid)
{
    std::pair<CallDescriptorsMap::iterator, CallDescriptorsMap::iterator>
            range = m_callDescriptors.equal_range(pc);

    for(CallDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
        if(it->second.pid == pid)
            return &it->second.signal;
    }

    CallDescriptor descriptor = { pid, FunctionMonitor::CallSignal() };
    CallDescriptorsMap::iterator it =
            m_newCallDescriptors.insert(std::make_pair(pc, descriptor));
    return &it->second.signal;
}


void FunctionMonitorState::slotCall(S2EExecutionState *state, uint64_t pc)
{
    target_ulong pid = state->getPid();
    target_ulong eip = state->getPc();

    if (!m_newCallDescriptors.empty()) {
        m_callDescriptors.insert(m_newCallDescriptors.begin(), m_newCallDescriptors.end());
        m_newCallDescriptors.clear();
    }

    /* Issue signals attached to all calls (pc==-1 means catch-all) */
    if (!m_callDescriptors.empty()) {
        std::pair<CallDescriptorsMap::iterator, CallDescriptorsMap::iterator>
                range = m_callDescriptors.equal_range((uint64_t)-1);
        for(CallDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
            CallDescriptor cd = (*it).second;
            if (m_plugin->m_monitor) {
                pid = m_plugin->m_monitor->getPid(state, pc);
            }
            if(it->second.pid == (uint64_t)-1 || it->second.pid == pid) {
                cd.signal.emit(state, this);
            }
        }
        if (!m_newCallDescriptors.empty()) {
            m_callDescriptors.insert(m_newCallDescriptors.begin(), m_newCallDescriptors.end());
            m_newCallDescriptors.clear();
        }
    }

    /* Issue signals attached to specific calls */
    if (!m_callDescriptors.empty()) {
        std::pair<CallDescriptorsMap::iterator, CallDescriptorsMap::iterator>
                range;

        range = m_callDescriptors.equal_range(eip);
        for(CallDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
            CallDescriptor cd = (*it).second;
            if (m_plugin->m_monitor) {
                pid = m_plugin->m_monitor->getPid(state, pc);
            }
            if(it->second.pid == (uint64_t)-1 || it->second.pid == pid) {
                cd.signal.emit(state, this);
            }
        }
        if (!m_newCallDescriptors.empty()) {
            m_callDescriptors.insert(m_newCallDescriptors.begin(), m_newCallDescriptors.end());
            m_newCallDescriptors.clear();
        }
    }
}

/**
 *  A call handler can invoke this function to register a return handler.
 *  XXX: We assume that the passed execution state corresponds to the state in which
 *  this instance of FunctionMonitorState is used.
 */
void FunctionMonitorState::registerReturnSignal(S2EExecutionState *state, FunctionMonitor::ReturnSignal &sig)
{
    if(sig.empty()) {
        return;
    }

    uint32_t sp;

#ifdef TARGET_ARM
    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[13]),
                                             &sp, sizeof(target_ulong));
#elif defined(TARGET_I386)
    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ESP]),
                                             &sp, sizeof(target_ulong));
#else
    assert(false);
#endif

    uint64_t pid = state->getPid();
    if (m_plugin->m_monitor) {
        pid = m_plugin->m_monitor->getPid(state, state->getPc());
    }

    if(!ok) {
        m_plugin->s2e()->getWarningsStream(state)
            << "Function call with symbolic SP!" << std::endl
            << "  PC=" << hexval(state->getPc()) << " PID=" << hexval(pid) << std::endl;
        return;
    }

    ReturnDescriptor descriptor = {pid, sig };
    m_returnDescriptors.insert(std::make_pair(sp, descriptor));
}

/**
 *  When emitSignal is false, this function simply removes all the return descriptors
 * for the current stack pointer. This can be used when a return handler manually changes the
 * program counter and/or wants to exit to the cpu loop and avoid being called again.
 *
 *  Note: all the return handlers will be erased if emitSignal is false, not just the one
 * that issued the call. Also note that it is not possible to return from the handler normally
 * whenever this function is called from within a return handler.
 */
void FunctionMonitorState::slotRet(S2EExecutionState *state, uint64_t pc, bool emitSignal)
{

target_ulong pid;
target_ulong sp;
#ifdef TARGET_ARM
	assert(m_plugin->m_monitor);
	pid = m_plugin->m_monitor->getPid(state, pc);

    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[13]),
                                             &sp, sizeof(target_ulong));
    if(!ok) {
        target_ulong pc = state->readCpuState(CPU_OFFSET(regs[15]),
                                               8*sizeof(target_ulong));
        m_plugin->s2e()->getWarningsStream(state)
            << "Function return with symbolic ESP!" << std::endl
            << "  PC=" << hexval(pc) << /*" PID=" << hexval(pid) <<*/ std::endl;
        return;
    }
#elif defined(TARGET_I386)
	pid = state->readCpuState(CPU_OFFSET(cr[3]), 8*sizeof(target_ulong));
    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ESP]),
                                             &sp, sizeof(target_ulong));
    if(!ok) {
    	target_ulong pc = state->readCpuState(CPU_OFFSET(eip),
                                               8*sizeof(target_ulong));
        m_plugin->s2e()->getWarningsStream(state)
            << "Function return with symbolic ESP!" << std::endl
            << "  PC=" << hexval(pc) << /*" PID=" << hexval(pid) <<*/ std::endl;
        return;
    }
#else
	assert(false);
#endif

    if (m_returnDescriptors.empty()) {
        return;
    }

    //m_plugin->s2e()->getDebugStream() << "ESP AT RETURN 0x" << std::hex << esp <<
    //        " plgstate=0x" << this << " EmitSignal=" << emitSignal <<  std::endl;

    bool finished = true;

    do {
        finished = true;
        std::pair<ReturnDescriptorsMap::iterator, ReturnDescriptorsMap::iterator>
                range = m_returnDescriptors.equal_range(sp);
        for(ReturnDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
            if (m_plugin->m_monitor) {
                pid = m_plugin->m_monitor->getPid(state, pc);
            }

            if(it->second.pid == pid) {
                if (emitSignal) {
                    it->second.signal.emit(state);
                }
                m_returnDescriptors.erase(it);
                finished = false;
                break;
            }
        }
    } while(!finished);

}


} // namespace plugins
} // namespace s2e
