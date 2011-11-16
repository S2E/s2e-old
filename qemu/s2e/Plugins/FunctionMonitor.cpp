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
        uint64_t eip, uint64_t cr3)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->getCallSignal(eip, cr3);
}

void FunctionMonitor::slotTranslateBlockEnd(ExecutionSignal *signal,
                                      S2EExecutionState *state,
                                      TranslationBlock *tb,
                                      uint64_t pc, bool, uint64_t)
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

void FunctionMonitor::disconnect(S2EExecutionState *state, const ModuleDescriptor &desc)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->disconnect(desc);
}

//See notes for slotRet to see how to use this function.
void FunctionMonitor::eraseSp(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->slotRet(state, pc, false);
}

void FunctionMonitor::registerReturnSignal(S2EExecutionState *state, FunctionMonitor::ReturnSignal &sig)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);
    plgState->registerReturnSignal(state, sig);
}


void FunctionMonitor::slotRet(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->slotRet(state, pc, true);
}

#if 0
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
#endif

FunctionMonitorState::FunctionMonitorState()
{

}

FunctionMonitorState::~FunctionMonitorState()
{

}

FunctionMonitorState* FunctionMonitorState::clone() const
{
    FunctionMonitorState *ret = new FunctionMonitorState(*this);
    m_plugin->s2e()->getDebugStream() << "Forking FunctionMonitorState ret=" << hexval(ret) << '\n';
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
        uint64_t eip, uint64_t cr3)
{
    std::pair<CallDescriptorsMap::iterator, CallDescriptorsMap::iterator>
            range = m_callDescriptors.equal_range(eip);

    for(CallDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
        if(it->second.cr3 == cr3)
            return &it->second.signal;
    }


    CallDescriptor descriptor = { cr3, FunctionMonitor::CallSignal() };
    CallDescriptorsMap::iterator it =
            m_newCallDescriptors.insert(std::make_pair(eip, descriptor));

    return &it->second.signal;
}


void FunctionMonitorState::slotCall(S2EExecutionState *state, uint64_t pc)
{
    target_ulong cr3 = state->getPid();
    target_ulong eip = state->getPc();

    if (!m_newCallDescriptors.empty()) {
        m_callDescriptors.insert(m_newCallDescriptors.begin(), m_newCallDescriptors.end());
        m_newCallDescriptors.clear();
    }

    /* Issue signals attached to all calls (eip==-1 means catch-all) */
    if (!m_callDescriptors.empty()) {
        std::pair<CallDescriptorsMap::iterator, CallDescriptorsMap::iterator>
                range = m_callDescriptors.equal_range((uint64_t)-1);
        for(CallDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
            CallDescriptor cd = (*it).second;
            if (m_plugin->m_monitor) {
                cr3 = m_plugin->m_monitor->getPid(state, pc);
            }
            if(it->second.cr3 == (uint64_t)-1 || it->second.cr3 == cr3) {
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
                cr3 = m_plugin->m_monitor->getPid(state, pc);
            }
            if(it->second.cr3 == (uint64_t)-1 || it->second.cr3 == cr3) {
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

    uint32_t esp;

    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ESP]),
                                             &esp, sizeof(target_ulong));
    if(!ok) {
        m_plugin->s2e()->getWarningsStream(state)
            << "Function call with symbolic ESP!\n"
            << "  EIP=" << hexval(state->getPc()) << " CR3=" << hexval(state->getPid()) << '\n';
        return;
    }

    uint64_t pid = state->getPid();
    if (m_plugin->m_monitor) {
        pid = m_plugin->m_monitor->getPid(state, state->getPc());
    }
    ReturnDescriptor descriptor = {pid, sig };
    m_returnDescriptors.insert(std::make_pair(esp, descriptor));
}

/**
 *  When emitSignal is false, this function simply removes all the return descriptors
 * for the current stack pointer. This can be used when a return handler manually changes the
 * program counter and/or wants to exit to the cpu loop and avoid being called again.
 *
 *  Note: all the return handlers will be erased if emitSignal is false, not just the one
 * that issued the call. Also note that it not possible to return from the handler normally
 * whenever this function is called from within a return handler.
 */
void FunctionMonitorState::slotRet(S2EExecutionState *state, uint64_t pc, bool emitSignal)
{
    target_ulong cr3 = state->readCpuState(CPU_OFFSET(cr[3]), 8*sizeof(target_ulong));

    target_ulong esp;
    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ESP]),
                                             &esp, sizeof(target_ulong));
    if(!ok) {
        target_ulong eip = state->readCpuState(CPU_OFFSET(eip),
                                               8*sizeof(target_ulong));
        m_plugin->s2e()->getWarningsStream(state)
            << "Function return with symbolic ESP!" << '\n'
            << "  EIP=" << hexval(eip) << " CR3=" << hexval(cr3) << '\n';
        return;
    }

    if (m_returnDescriptors.empty()) {
        return;
    }

    //m_plugin->s2e()->getDebugStream() << "ESP AT RETURN 0x" << std::hex << esp <<
    //        " plgstate=0x" << this << " EmitSignal=" << emitSignal <<  std::endl;

    bool finished = true;
    do {
        finished = true;
        std::pair<ReturnDescriptorsMap::iterator, ReturnDescriptorsMap::iterator>
                range = m_returnDescriptors.equal_range(esp);
        for(ReturnDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
            if (m_plugin->m_monitor) {
                cr3 = m_plugin->m_monitor->getPid(state, pc);
            }

            if(it->second.cr3 == cr3) {
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

void FunctionMonitorState::disconnect(const ModuleDescriptor &desc, CallDescriptorsMap &descMap)
{
    CallDescriptorsMap::iterator it = descMap.begin();
    while (it != descMap.end()) {
        uint64_t addr = (*it).first;
        const CallDescriptor &call = (*it).second;
        if (desc.Contains(addr) && desc.Pid == call.cr3) {
            CallDescriptorsMap::iterator it2 = it;
            ++it;
            descMap.erase(it2);
        }else {
            ++it;
        }
    }
}

//Disconnect all address that belong to desc.
//This is useful to unregister all handlers when a module is unloaded
void FunctionMonitorState::disconnect(const ModuleDescriptor &desc)
{

    disconnect(desc, m_callDescriptors);
    disconnect(desc, m_newCallDescriptors);

    //XXX: we assume there are no more return descriptors active when the module is unloaded
}


} // namespace plugins
} // namespace s2e
