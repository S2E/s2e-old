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
#include "sysemu.h"
}


#include "X86ExceptionInterceptor.h"
#include "ModuleExecutionDetector.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/S2EExecutor.h>


#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(X86ExceptionInterceptor, "X86ExceptionInterceptor S2E plugin", "",);

void X86ExceptionInterceptor::initialize()
{
    m_inited = false;

    m_tbTranslateConnection = s2e()->getCorePlugin()->onTranslateBlockStart.connect(
            sigc::mem_fun(*this, &X86ExceptionInterceptor::onTranslateBlockStart));
}

void X86ExceptionInterceptor::onTranslateBlockStart(ExecutionSignal *signal,
                                      S2EExecutionState *state,
                                      TranslationBlock *tb,
                                      uint64_t pc)
{
    if (!initializeExceptionHandlers(state)) {
        return;
    }

    Handler hdlr;
    hdlr.pc = pc;
    if (m_handlers.find(hdlr) == m_handlers.end()) {
        return;
    }

    signal->connect(sigc::mem_fun(*this, &X86ExceptionInterceptor::onExecuteBlockStart));
}


//Go through the IDT and extract the addresses of the exception handlers
bool X86ExceptionInterceptor::initializeExceptionHandlers(S2EExecutionState *state)
{
    if (m_inited) {
        return true;
    }

    m_idt.clear();
    m_gdt.clear();

    if (!X86Parser::getIdt(state, m_idt)) {
        return false;
    }

    if (!X86Parser::getGdt(state, m_gdt)) {
        return false;
    }

    //Register the relevant exception handlers
    registerHandler(state, DOUBLE_FAULT);
    registerHandler(state, STACK_FAULT);
    registerHandler(state, GPF);

    m_inited = true;

    return true;
}

bool X86ExceptionInterceptor::registerHandler(S2EExecutionState *state, EX86Exceptions idtVector)
{
    if ((unsigned)idtVector >= m_idt.size()) {
        return false;
    }

    const X86IDTEntry &entry = m_idt[idtVector];
    switch (entry.getType()) {
    case TASK_GATE:
        {
            uint16_t selector = entry.u_TaskGate.m_Selector;
            if (selector & 4) {
                //Reading from LDT
                break;
            }

            //Reading from GDT
            X86GDTEntry gdtEntry;
            X86TSS tss;

            if (!X86Parser::getGdtEntry(state, &gdtEntry, selector)) {
                return false;
            }

            if (!X86Parser::isTss(gdtEntry)) {
                return false;
            }

            uint32_t base = X86Parser::getBase(gdtEntry);
            if (!X86Parser::getTss(state, base, &tss)) {
                return false;
            }

            uint32_t eip = tss.m_EIP;

            Handler hdlr;
            hdlr.pc = eip;
            hdlr.idtVector = idtVector;
            m_handlers.insert(hdlr);

            s2e()->getDebugStream() << "Registered task hander at 0x" <<
                    std::hex << eip << std::endl;

        }
        break;

    case INT_GATE:
        {
            Handler hdlr;
            hdlr.pc = X86Parser::getOffset(entry);
            hdlr.idtVector = idtVector;
            m_handlers.insert(hdlr);

            s2e()->getDebugStream() << "Registered interrupt gate hander at 0x" <<
                    std::hex << hdlr.pc << std::endl;
        }
        break;

    case TRAP_GATE:
        {
            Handler hdlr;
            hdlr.pc = X86Parser::getOffset(entry);
            hdlr.idtVector = idtVector;
            m_handlers.insert(hdlr);

            s2e()->getDebugStream() << "Registered trap gate hander at 0x" <<
                    std::hex << hdlr.pc << std::endl;
        }
        break;

    default:
        s2e()->getDebugStream() << "Unhandled entry type " << entry.getType() << std::endl;
        return false;
        break;
    }
    return true;
}

void X86ExceptionInterceptor::onExecuteBlockStart(S2EExecutionState *state, uint64_t pc)
{
    std::cout << "Fired exception at " << std::hex << pc << std::dec << std::endl;

    Handler hdlr;
    hdlr.pc = pc;
    const Handlers::iterator it = m_handlers.find(hdlr);
    assert (it != m_handlers.end());

    const Handler &h = *it;

    switch(h.idtVector) {
        case GPF:
            s2e()->getMessagesStream() << "General protection fault" << std::endl;
            break;
        case STACK_FAULT:
            s2e()->getMessagesStream() << "Stack fault" << std::endl;
            break;
        case DOUBLE_FAULT:
            s2e()->getMessagesStream() << "Double fault" << std::endl;
            break;
    }

    const X86IDTEntry &entry = m_idt[h.idtVector];
    switch (entry.getType()) {
        case TASK_GATE:
            handleTaskGate(state, h);
            break;

        case INT_GATE:
            s2e()->getDebugStream() << "INT_GATE not handled yet" << std::endl;
            break;

        case TRAP_GATE:
            s2e()->getDebugStream() << "TRAP_GATE not handled yet" << std::endl;
            break;
    }
#if 0
    switch(h.idtVector) {
        case GPF:
            s2e()->getExecutor()->terminateStateEarly(*state, "General protection fault");
            break;
        case STACK_FAULT:
            s2e()->getExecutor()->terminateStateEarly(*state, "Stack fault");
            break;
        case DOUBLE_FAULT:
            s2e()->getExecutor()->terminateStateEarly(*state, "Double fault");
            break;
    }
#endif
}

void X86ExceptionInterceptor::handleTaskGate(S2EExecutionState *state, const Handler &hdlr)
{
    X86TSS tss, faultyTss;
    if (!X86Parser::getCurrentTss(state, &tss)) {
        s2e()->getDebugStream() << "Could not read current TSS" << std::endl;
        return;
    }

    //Determine what task triggered the fault
    X86GDTEntry gdtEntry;
    uint16_t faultyTssSel = tss.m_PreviousTaskLink;

    if (!X86Parser::getGdtEntry(state, &gdtEntry, faultyTssSel)) {
        s2e()->getDebugStream() << "Could not retrieve TSS descriptor for faulty task " << std::dec << faultyTssSel << std::endl;
        return;
    }

    uint32_t faultyTssBase = X86Parser::getBase(gdtEntry);
    if (!X86Parser::getTss(state, faultyTssBase, &faultyTss)) {
        s2e()->getDebugStream() <<  "Could not get faulty TSS" << std::endl;
        return;
    }

    faultyTss.dumpInfo(s2e()->getDebugStream());

    ModuleExecutionDetector *m_exec = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");

    if (m_exec) {
        m_exec->dumpMemory(state, s2e()->getDebugStream(), faultyTss.m_ESP, 512);
    }else {
        state->dumpStack(512, faultyTss.m_ESP);
    }

    //Dump the IDT table for page fault
    //XXX: generalize this
    uint32_t pfhdlr = X86Parser::getOffset(m_idt[14]) ;
    std::ostream &os = s2e()->getDebugStream();
    os << "PF handler is at 0x" << std::hex << pfhdlr << std::endl;

    uint8_t chr;
    if (!state->readMemoryConcrete(pfhdlr, &chr, sizeof(chr))) {
        os << "Could not read here" << std::endl;
    }
}

//////////////////////////////

void X86TSS::dumpInfo(std::ostream &o)
{
  o << std::hex;
  o << "============ TSS DUMP =============" <<std::endl;
  o << "SS0:ESP0=0x" << m_SS0 << ":0x" << m_ESP0 << std::endl;
  o << "SS1:ESP1=0x" << m_SS1 << ":0x" << m_ESP1 << std::endl;
  o << "SS2:ESP2=0x" << m_SS2 << ":0x" << m_ESP2 << std::endl << std::endl;


  o << "<<EIP=0x" << m_EIP << ">>" << std::endl;
  o << "DS=0x" << m_DS << " ";
  o << "ES=0x" << m_ES << " ";
  o << "SS=0x" << m_SS << " ";
  o << "FS=0x" << m_FS << " ";
  o << "GS=0x" << m_GS << " " << std::endl;

  o << "EAX=0x" << m_EAX << " ";
  o << "EBX=0x" << m_EBX << " ";
  o << "ECX=0x" << m_ECX << " ";
  o << "EDX=0x" << m_EDX << " " << std::endl;
  o << "ESI=0x" << m_ESI << " ";
  o << "EDI=0x" << m_EDI << " ";
  o << "EBP=0x" << m_EBP << " ";
  o << "ESP=0x" << m_ESP << " " << std::endl;
}

bool X86Parser::getIdt(S2EExecutionState *state, IDT &table)
{
    uint32_t idtBase;
    uint32_t idtLimit;

    idtBase =  state->readCpuState(offsetof(CPUState, idt.base), 8 * sizeof(idtBase));
    idtLimit =  state->readCpuState(offsetof(CPUState, idt.limit), 8 * sizeof(idtLimit));

    unsigned maxHdlrs = (idtLimit+1) / 8;

    //Struc has to be packed...
    assert(sizeof(X86IDTEntry) == 8);

    X86IDTEntry entry;

    for (unsigned i=0; i<maxHdlrs; i++) {
        if (!state->readMemoryConcrete(idtBase + i * sizeof(X86IDTEntry), &entry, sizeof(X86IDTEntry))) {
            g_s2e->getDebugStream() << "Could not read IDT entry " << i  << std::endl;
            return false;
        }else {
            table.push_back(entry);
        }
    }
    return true;
}

bool X86Parser::getGdt(S2EExecutionState *state, GDT &table)
{
    target_ulong gdtBase;
    uint32_t gdtLimit;

    gdtBase =  state->readCpuState(offsetof(CPUState, gdt.base), 8 * sizeof(gdtBase));
    gdtLimit = state->readCpuState(offsetof(CPUState, gdt.limit), 8 * sizeof(gdtLimit));

    unsigned maxHdlrs = (gdtLimit+1) / 8;

    //Struc has to be packed...
    assert(sizeof(X86GDTEntry) == 8);

    X86GDTEntry entry;

    for (unsigned i=0; i<maxHdlrs; i++) {
        if (!state->readMemoryConcrete(gdtBase + i * sizeof(X86GDTEntry), &entry, sizeof(X86GDTEntry))) {
            g_s2e->getDebugStream() << "Could not read GDT entry " << i  << std::endl;
            return false;
        }else {
            table.push_back(entry);
        }
    }
    return true;
}

bool X86Parser::getGdtEntry(S2EExecutionState *state, X86GDTEntry *gdtEntry, uint16_t selector)
{
  target_ulong gdtBase;
  uint32_t gdtLimit;

  gdtBase =  state->readCpuState(offsetof(CPUState, gdt.base), 8 * sizeof(gdtBase));
  gdtLimit = state->readCpuState(offsetof(CPUState, gdt.limit), 8 * sizeof(gdtLimit));

  unsigned maxHdlrs = (gdtLimit+1) / 8;

  //Struc has to be packed...
  assert(sizeof(X86GDTEntry) == 8);

  if (selector & 4) {
    //LDT entry, bad
    g_s2e->getDebugStream() << "Requesting and LDT entry"  << std::endl;
    return false;
  }

  uint16_t entry = selector >> 3;

  if (entry >= maxHdlrs) {
    g_s2e->getDebugStream() << "GDT is too small for entry " << entry  << std::endl;
    return false;
  }

  if (!state->readMemoryConcrete(gdtBase + entry * sizeof(X86GDTEntry), gdtEntry, sizeof(X86GDTEntry))) {
      g_s2e->getDebugStream() << "Could not read GDT entry " << entry  << std::endl;
      return false;
  }

  return true;
}

bool X86Parser::getTss(S2EExecutionState *state, uint32_t base, X86TSS *tss)
{
    if (!state->readMemoryConcrete(base, tss, sizeof(X86TSS))) {
        g_s2e->getDebugStream() << "Could not read TSS at 0x" << std::hex << base << std::endl;
        return false;
    }

    return true;
}

bool X86Parser::getCurrentTss(S2EExecutionState *state, X86TSS *tss)
{
  target_ulong base;
  unsigned limit;

  base =  state->readCpuState(offsetof(CPUState, tr.base), 8 * sizeof(base));
  limit = state->readCpuState(offsetof(CPUState, tr.limit), 8 * sizeof(limit));

  assert(sizeof(X86TSS) == 0x68);
  if (limit < sizeof(X86TSS)) {
    g_s2e->getDebugStream() << "Current task register points to an invalid TSS"  << std::endl;
    return false;
  }

  return getTss(state, base, tss);
}

uint32_t X86Parser::getBase(const X86GDTEntry &E)
{
  return (E.m_BaseHigh << 24) | (E.m_BaseMiddle<<16) | E.m_BaseLow;
}

uint32_t X86Parser::getLimit(const X86GDTEntry &E)
{
  return (E.m_LimitHigh<<16) | E.m_LimitLow;
}

uint32_t X86Parser::getOffset(const X86IDTEntry &E)
{
  return (E.u_TrapIntGate.m_OffsetHigh<<16) | E.u_TrapIntGate.m_OffsetLow;
}

bool X86Parser::isTss(const X86GDTEntry &E)
{
  return !E.m_NotSystem && ((E.m_Type & 9) == 9);
}


} // namespace plugins
} // namespace s2e
