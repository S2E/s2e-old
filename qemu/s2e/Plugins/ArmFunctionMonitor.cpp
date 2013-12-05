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

extern "C"
{
#include "config.h"
#include "qemu-common.h"
}

#include "FunctionMonitor.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>

/**
 * WARNING: If you use this plugin together with the RemoteMemory plugin, know what you are doing!
 *          This plugin inspects memory contents of QEMU to find instructions, it will fail if instructions
 *          are inserted via the onDataAccess hook!
 *
 * TODO:    try to make the FunctionMonitorState arch-independent
 */



namespace s2e {
  namespace plugins {

S2E_DEFINE_PLUGIN(FunctionMonitor, "ARM function calls/returns monitoring plugin", "FunctionMonitor" );

void ARMFunctionMonitor::initialize()
{
    s2e()->getCorePlugin()->onTranslateBlockEnd.connect(
            sigc::mem_fun(*this, &ARMFunctionMonitor::slotTranslateBlockEnd));

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
            sigc::mem_fun(*this, &ARMFunctionMonitor::slotTranslateBlockStart));
//TODO: TranslateJumpStart is not (yet) emitted on ARM
//    s2e()->getCorePlugin()->onTranslateJumpStart.connect(
//            sigc::mem_fun(*this, &FunctionMonitor::slotTranslateJumpStart));

    m_monitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));
}

////XXX: Implement onModuleUnload to automatically clear all call signals
ARMFunctionMonitor::CallSignal* ARMFunctionMonitor::getCallSignal(
        S2EExecutionState *state,
        uint64_t eip, uint64_t cr3)
{
    DECLARE_PLUGINSTATE(ARMFunctionMonitorState, state);

    return plgState->getCallSignal(eip, cr3);
}

void ARMFunctionMonitor::callFunction(S2EExecutionState *state,
                                        uint64_t pc,
                                        uint64_t function_address)
{
    uint32_t return_address;

    // Default branch and link behavior
    return_address = pc + 4;

    // Thumb short-branch check
    uint64_t thumb_flag = state->readCpuState(CPU_OFFSET(thumb), klee::Expr::Int32);
    if (thumb_flag) {
        klee::ref<klee::Expr> expr = state->readMemory(pc, klee::Expr::Int16, s2e::S2EExecutionState::PhysicalAddress);

        if (!isa<klee::ConstantExpr>(expr))
        {
            s2e()->getWarningsStream()
               << "[ARMFunctionMonitor]: Found symbolic instruction at address 0x"
                << hexval(pc) << '\n';
            return;
        }
        uint32_t opcode = static_cast<uint32_t>(cast<klee::ConstantExpr>(expr)->getZExtValue());

        if ((opcode & 0xFF00) == 0x4700) {
            // BL/BLX register
            return_address = pc + 2;
        }
    }

    // Keep track of functions entry-points and return-points
    if (m_funcs.find(function_address) == m_funcs.end()) {
        m_funcs.insert(std::make_pair(function_address, return_address));
        m_returns.insert(std::make_pair(return_address, function_address));

        s2e()->getDebugStream()
                << "[ARMFunctionMonitor]: Detected function call at "
                << hexval(pc) << " to " << hexval(function_address)
                << ", return to " << hexval(return_address) << "\n";
    }

    // Emit a proper CallSignal for this subroutine
    slotCall(state, function_address);

}

/*
 * This method is executed at runtime to get the real target of an indirect branch.
 */
void ARMFunctionMonitor::callFunctionIndirect(S2EExecutionState *state,
                                                uint64_t pc,
                                                target_ulong reg)
{
    uint32_t address;
    assert (reg < 15);
    if (!state->readCpuRegisterConcrete(CPU_OFFSET(regs[reg]), &address, sizeof(address))) {
      s2e()->getWarningsStream()
          << "[ARMFunctionMonitor]: Found symbolic register value at address "
          << hexval(pc)  << '\n';
      return;
    }
    callFunction(state, pc, address);
}

/*
 * This method checks all block ends to gather:
 *   - the destination of the branch&link (function entry-point)
 *   - the next pc (function return-point)
 */
void ARMFunctionMonitor::slotTranslateBlockEnd(ExecutionSignal *signal,
    S2EExecutionState *state, TranslationBlock *tb, uint64_t pc,
    bool is_static_target, uint64_t static_target_pc)
{
    // Check if we are jumping back after a function
    if (is_static_target &&
              m_returns.find(static_target_pc) != m_returns.end()) {
        signal->connect(sigc::mem_fun(*this, &ARMFunctionMonitor::slotRet));
        return;
    }

    // Otherwise decode the branch target and record function details
    bool isThumb = (tb->instruction_set == INSTRUCTION_SET_THUMB) ? true : false;
    klee::ref<klee::Expr> expr = state->readMemory(pc, klee::Expr::Int32,
        s2e::S2EExecutionState::PhysicalAddress);

    if (!isa<klee::ConstantExpr>(expr)) {
      s2e()->getWarningsStream()
          << "[ARMFunctionMonitor]: Found symbolic instruction at address 0x"
          << hexval(pc) << '\n';
      return;
    }

    target_ulong opcode = static_cast<target_ulong>(cast<klee::ConstantExpr>(expr)->getZExtValue());

    if (!isThumb) {
        if ((opcode & 0x0F000000) == 0x0B000000 ||          // BL  immediate
            (opcode & 0xFE000000) == 0xFA000000)  {         // BLX immediate
            assert(is_static_target);
            signal->connect(sigc::bind(sigc::mem_fun(*this, &ARMFunctionMonitor::callFunction),
                                        static_target_pc));
        } else if ((opcode & 0x0FF000F0) == 0x01200030)     // BLX<c> register
        {
            assert(!is_static_target);
            uint32_t reg = opcode & 0xf;
            if (reg < 15) {
              signal->connect(sigc::bind(sigc::mem_fun(*this, &ARMFunctionMonitor::callFunctionIndirect),
                                          reg));
            }
        }
    }
    else if (isThumb)
    {
        if ((opcode & 0xF800F800) == 0xF800F000) {          // BL<c> immediate
            assert(is_static_target);
            uint32_t target_address = static_target_pc;
            uint32_t h = (opcode >> (11 + 16)) & 0x3;
            if (h == 0b01)
            {
                target_address &= 0xFFFFFFFC;
            }
            signal->connect(sigc::bind(sigc::mem_fun(*this, &ARMFunctionMonitor::callFunction),
                                      target_address));

        } else  if ((opcode & 0x0000FF80) == 0x4780) {      //BLX<c> register
            assert(!is_static_target);
            target_ulong reg = (opcode >> 3) & 0xf;
            if (reg < 15) {
              signal->connect(sigc::bind(sigc::mem_fun(*this, &ARMFunctionMonitor::callFunctionIndirect),
                                        reg));
            }
        }
    }
}

void
ARMFunctionMonitor::slotTranslateBlockStart(ExecutionSignal *signal,
    S2EExecutionState *state, TranslationBlock *tb, uint64_t pc)
{
    /* Check if this is the return point of a previous-seen branch */
    std::map< uint64_t, uint64_t >::iterator itr = m_returns.find(pc);
    if (itr != m_returns.end())
    {
        //TODO: recursive functions?
        signal->connect(sigc::mem_fun(*this, &ARMFunctionMonitor::slotRet));
    }
}


void ARMFunctionMonitor::slotCall(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(ARMFunctionMonitorState, state);

    return plgState->slotCall(state, pc);
}


void ARMFunctionMonitor::slotRet(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(ARMFunctionMonitorState, state);

    return plgState->slotRet(state, pc, true);
}

void ARMFunctionMonitor::slotTraceCall(S2EExecutionState *state, ARMFunctionMonitorState *fns)
{
    static int f = 0;

    ARMFunctionMonitor::ReturnSignal returnSignal;
    returnSignal.connect(sigc::bind(sigc::mem_fun(*this, &ARMFunctionMonitor::slotTraceRet), f));
    fns->registerReturnSignal(state, returnSignal);

    s2e()->getMessagesStream(state) << "Calling function " << f
                << " at " << hexval(state->getPc()) << '\n';
    ++f;
}

void ARMFunctionMonitor::slotTraceRet(S2EExecutionState *state, int f)
{
    s2e()->getMessagesStream(state) << "Returning from function "
                << f << '\n';
}

//See notes for slotRet to see how to use this function.
void ARMFunctionMonitor::eraseSp(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(ARMFunctionMonitorState, state);

    return plgState->slotRet(state, pc, false);
}

void ARMFunctionMonitor::disconnect(S2EExecutionState *state, const ModuleDescriptor &desc)
{
    DECLARE_PLUGINSTATE(ARMFunctionMonitorState, state);

    return plgState->disconnect(desc);
}

ARMFunctionMonitorState::ARMFunctionMonitorState()
{

}

ARMFunctionMonitorState::~ARMFunctionMonitorState()
{

}

ARMFunctionMonitorState* ARMFunctionMonitorState::clone() const
{
    ARMFunctionMonitorState *ret = new ARMFunctionMonitorState(*this);
//    m_plugin->s2e()->getDebugStream() << "Forking ARMFunctionMonitorState ret=" << std::hex << ret << '\n';
//    assert(ret->m_returnDescriptors.size() == m_returnDescriptors.size());
    return ret;
}

PluginState *ARMFunctionMonitorState::factory(Plugin *p, S2EExecutionState *s)
{
    ARMFunctionMonitorState *ret = new ARMFunctionMonitorState();
    ret->m_plugin = static_cast<ARMFunctionMonitor*>(p);
    return ret;
}

ARMFunctionMonitor::CallSignal* ARMFunctionMonitorState::getCallSignal(
        uint64_t pc, uint64_t pid)
{
    std::pair<CallDescriptorsMap::iterator, CallDescriptorsMap::iterator>
            range = m_callDescriptors.equal_range(pc);

    for(CallDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
        if(it->second.cr3 == pid)
            return &it->second.signal;
    }

    CallDescriptor descriptor = { pid, FunctionMonitor::CallSignal() };
    CallDescriptorsMap::iterator it =
            m_newCallDescriptors.insert(std::make_pair(pc, descriptor));
    return &it->second.signal;
}


void ARMFunctionMonitorState::slotCall(S2EExecutionState *state, uint64_t pc)
{
    target_ulong pid = state->getPid();
    target_ulong eip = pc;

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
                pid = m_plugin->m_monitor->getPid(state, pc);
            }
            if(it->second.cr3 == (uint64_t)-1 || it->second.cr3 == pid) {
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
            if(it->second.cr3 == (uint64_t)-1 || it->second.cr3 == pid) {
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
 *  this instance of ARMFunctionMonitorState is used.
 */
void ARMFunctionMonitorState::registerReturnSignal(S2EExecutionState *state, ARMFunctionMonitor::ReturnSignal &sig)
{
    if(sig.empty()) {
        return;
    }

    uint32_t sp;

    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[13]),
                                             &sp, sizeof(target_ulong));
    if(!ok) {
        m_plugin->s2e()->getWarningsStream(state)
            << "Function call with symbolic SP!" << '\n'
            << "  EIP=" << hexval(state->getPc()) << " SP=" << hexval(state->getPid()) << '\n';
        return;
    }

    uint64_t pid = state->getPid();
    if (m_plugin->m_monitor) {
        pid = m_plugin->m_monitor->getPid(state, state->getPc());
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
 * that issued the call. Also note that it not possible to return from the handler normally
 * whenever this function is called from within a return handler.
 */
void ARMFunctionMonitorState::slotRet(S2EExecutionState *state, uint64_t pc, bool emitSignal)
{
    // dummy
    uint64_t cr3 = -1; //state->readCpuState(CPU_OFFSET(cr[3]), 8*sizeof(target_ulong));

    target_ulong sp;
    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[13]),
                                             &sp, sizeof(target_ulong));
    if(!ok) {
        target_ulong eip = state->readCpuState(CPU_OFFSET(regs[15]),
                                               8*sizeof(target_ulong));
        m_plugin->s2e()->getWarningsStream(state)
            << "Function return with symbolic SP!" << '\n'
            << "  EIP=" << hexval(eip) << " CR3=" << hexval(cr3) << '\n';
        return;
    }

    if (m_returnDescriptors.empty()) {
        return;
    }

    //m_plugin->s2e()->getDebugStream() << "ESP AT RETURN 0x" << std::hex << esp <<
    //        " plgstate=0x" << this << " EmitSignal=" << emitSignal <<  '\n';

    bool finished = true;
    do {
        finished = true;
        std::pair<ReturnDescriptorsMap::iterator, ReturnDescriptorsMap::iterator>
                range = m_returnDescriptors.equal_range(sp);
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

void ARMFunctionMonitorState::disconnect(const ModuleDescriptor &desc, CallDescriptorsMap &descMap)
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
void ARMFunctionMonitorState::disconnect(const ModuleDescriptor &desc)
{

    disconnect(desc, m_callDescriptors);
    disconnect(desc, m_newCallDescriptors);

    //XXX: we assume there are no more return descriptors active when the module is unloaded
}

  }// namespace plugins
} // namespace s2e
