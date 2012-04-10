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


#include "X86ExceptionInterceptor.h"
#include "ModuleExecutionDetector.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/S2EExecutor.h>
#include <s2e/s2e_config.h>

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

            s2e()->getDebugStream() << "Registered task hander at " <<
                    hexval(eip) << '\n';

        }
        break;

    case INT_GATE:
        {
            Handler hdlr;
            hdlr.pc = X86Parser::getOffset(entry);
            hdlr.idtVector = idtVector;
            m_handlers.insert(hdlr);

            s2e()->getDebugStream() << "Registered interrupt gate hander at " <<
                    hexval(hdlr.pc) << '\n';
        }
        break;

    case TRAP_GATE:
        {
            Handler hdlr;
            hdlr.pc = X86Parser::getOffset(entry);
            hdlr.idtVector = idtVector;
            m_handlers.insert(hdlr);

            s2e()->getDebugStream() << "Registered trap gate hander at " <<
                    hexval(hdlr.pc) << '\n';
        }
        break;

    default:
        s2e()->getDebugStream() << "Unhandled entry type " << entry.getType() << '\n';
        return false;
        break;
    }
    return true;
}

void X86ExceptionInterceptor::onExecuteBlockStart(S2EExecutionState *state, uint64_t pc)
{
    s2e()->getMessagesStream() << "Fired exception at " << hexval(pc) << '\n';

    Handler hdlr;
    hdlr.pc = pc;
    const Handlers::iterator it = m_handlers.find(hdlr);
    assert (it != m_handlers.end());

    const Handler &h = *it;

    switch(h.idtVector) {
        case GPF:
            s2e()->getMessagesStream() << "General protection fault\n";
            break;
        case STACK_FAULT:
            s2e()->getMessagesStream() << "Stack fault\n";
            break;
        case DOUBLE_FAULT:
            s2e()->getMessagesStream() << "Double fault\n";
            break;
    }

    const X86IDTEntry &entry = m_idt[h.idtVector];
    switch (entry.getType()) {
        case TASK_GATE:
            handleTaskGate(state, h);
            break;

        case INT_GATE:
            s2e()->getDebugStream() << "INT_GATE not handled yet\n";
            break;

        case TRAP_GATE:
            s2e()->getDebugStream() << "TRAP_GATE not handled yet\n";
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
        s2e()->getDebugStream() << "Could not read current TSS\n";
        return;
    }

    //Determine what task triggered the fault
    X86GDTEntry gdtEntry;
    uint16_t faultyTssSel = tss.m_PreviousTaskLink;

    if (!X86Parser::getGdtEntry(state, &gdtEntry, faultyTssSel)) {
        s2e()->getDebugStream() << "Could not retrieve TSS descriptor for faulty task " << faultyTssSel << '\n';
        return;
    }

    uint32_t faultyTssBase = X86Parser::getBase(gdtEntry);
    if (!X86Parser::getTss(state, faultyTssBase, &faultyTss)) {
        s2e()->getDebugStream() <<  "Could not get faulty TSS\n";
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
    llvm::raw_ostream &os = s2e()->getDebugStream();
    os << "PF handler is at " << hexval(pfhdlr) << '\n';

    uint8_t chr;
    if (!state->readMemoryConcrete(pfhdlr, &chr, sizeof(chr))) {
        os << "Could not read here\n";
    }
}

//////////////////////////////

void X86TSS::dumpInfo(llvm::raw_ostream &o)
{
  o << "============ TSS DUMP =============\n";
  o << "SS0:ESP0=" << hexval(m_SS0) << ":" << hexval(m_ESP0) << '\n';
  o << "SS1:ESP1=" << hexval(m_SS1) << ":" << hexval(m_ESP1) << '\n';
  o << "SS2:ESP2=" << hexval(m_SS2) << ":" << hexval(m_ESP2) << "\n\n";


  o << "<<EIP=" << hexval(m_EIP) << ">>\n";
  o << "DS=" << hexval(m_DS) << " ";
  o << "ES=" << hexval(m_ES) << " ";
  o << "SS=" << hexval(m_SS) << " ";
  o << "FS=" << hexval(m_FS) << " ";
  o << "GS=" << hexval(m_GS) << " \n";

  o << "EAX=" << hexval(m_EAX) << " ";
  o << "EBX=" << hexval(m_EBX) << " ";
  o << "ECX=" << hexval(m_ECX) << " ";
  o << "EDX=" << hexval(m_EDX) << " \n";
  o << "ESI=" << hexval(m_ESI) << " ";
  o << "EDI=" << hexval(m_EDI) << " ";
  o << "EBP=" << hexval(m_EBP) << " ";
  o << "ESP=" << hexval(m_ESP) << " \n";
}

bool X86Parser::getIdt(S2EExecutionState *state, IDT &table)
{
    uint32_t idtBase;
    uint32_t idtLimit;

    idtBase =  state->readCpuState(offsetof(CPUX86State, idt.base), 8 * sizeof(idtBase));
    idtLimit =  state->readCpuState(offsetof(CPUX86State, idt.limit), 8 * sizeof(idtLimit));

    unsigned maxHdlrs = (idtLimit+1) / 8;

    //Struc has to be packed...
    assert(sizeof(X86IDTEntry) == 8);

    X86IDTEntry entry;

    for (unsigned i=0; i<maxHdlrs; i++) {
        if (!state->readMemoryConcrete(idtBase + i * sizeof(X86IDTEntry), &entry, sizeof(X86IDTEntry))) {
            g_s2e->getDebugStream() << "Could not read IDT entry " << i  << '\n';
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

    gdtBase =  state->readCpuState(offsetof(CPUX86State, gdt.base), 8 * sizeof(gdtBase));
    gdtLimit = state->readCpuState(offsetof(CPUX86State, gdt.limit), 8 * sizeof(gdtLimit));

    unsigned maxHdlrs = (gdtLimit+1) / 8;

    //Struc has to be packed...
    assert(sizeof(X86GDTEntry) == 8);

    X86GDTEntry entry;

    for (unsigned i=0; i<maxHdlrs; i++) {
        if (!state->readMemoryConcrete(gdtBase + i * sizeof(X86GDTEntry), &entry, sizeof(X86GDTEntry))) {
            g_s2e->getDebugStream() << "Could not read GDT entry " << i  << '\n';
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

  gdtBase =  state->readCpuState(offsetof(CPUX86State, gdt.base), 8 * sizeof(gdtBase));
  gdtLimit = state->readCpuState(offsetof(CPUX86State, gdt.limit), 8 * sizeof(gdtLimit));

  unsigned maxHdlrs = (gdtLimit+1) / 8;

  //Struc has to be packed...
  assert(sizeof(X86GDTEntry) == 8);

  if (selector & 4) {
    //LDT entry, bad
    g_s2e->getDebugStream() << "Requesting and LDT entry\n";
    return false;
  }

  uint16_t entry = selector >> 3;

  if (entry >= maxHdlrs) {
    g_s2e->getDebugStream() << "GDT is too small for entry " << entry  << '\n';
    return false;
  }

  if (!state->readMemoryConcrete(gdtBase + entry * sizeof(X86GDTEntry), gdtEntry, sizeof(X86GDTEntry))) {
      g_s2e->getDebugStream() << "Could not read GDT entry " << entry  << '\n';
      return false;
  }

  return true;
}

bool X86Parser::getTss(S2EExecutionState *state, uint32_t base, X86TSS *tss)
{
    if (!state->readMemoryConcrete(base, tss, sizeof(X86TSS))) {
        g_s2e->getDebugStream() << "Could not read TSS at " << hexval(base) << '\n';
        return false;
    }

    return true;
}

bool X86Parser::getCurrentTss(S2EExecutionState *state, X86TSS *tss)
{
  target_ulong base;
  unsigned limit;

  base =  state->readCpuState(offsetof(CPUX86State, tr.base), 8 * sizeof(base));
  limit = state->readCpuState(offsetof(CPUX86State, tr.limit), 8 * sizeof(limit));

  assert(sizeof(X86TSS) == 0x68);
  if (limit < sizeof(X86TSS)) {
    g_s2e->getDebugStream() << "Current task register points to an invalid TSS\n";
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
