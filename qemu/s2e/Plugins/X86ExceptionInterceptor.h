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

#ifndef S2E_PLUGINS_X86EI_H
#define S2E_PLUGINS_X86EI_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {

enum EX86Exceptions {
    DOUBLE_FAULT=8,
    STACK_FAULT=12,
    GPF=13
};

enum EX86IDTType{
  TASK_GATE=5,
  INT_GATE=6,
  TRAP_GATE=7
};


struct X86TrapIntGate {
  uint16_t m_OffsetLow;
  uint16_t m_Selector;
  uint8_t m_Zero1;
  unsigned m_Type:5;
  unsigned m_Dpl: 2;
  unsigned m_Present:1;
  uint16_t m_OffsetHigh;
}__attribute__((packed));

struct X86TaskGate {
  uint16_t m_Reserved1;
  uint16_t m_Selector;
  uint8_t m_Reserved2;
  unsigned m_Type:5;
  unsigned m_Dpl: 2;
  unsigned m_Present:1;
}__attribute__((packed));

struct X86IDTEntry {
  union {
    X86TrapIntGate u_TrapIntGate;
    X86TaskGate u_TaskGate;
  };
  EX86IDTType getType() const {
    return (EX86IDTType)(u_TaskGate.m_Type & 7);
  }
}__attribute__((packed));

struct X86GDTEntry {
  uint16_t m_LimitLow;
  uint16_t m_BaseLow;
  uint8_t m_BaseMiddle;
  unsigned m_Type:4;
  unsigned m_NotSystem:1;
  unsigned m_Dpl: 2;
  unsigned m_Present:1;
  unsigned m_LimitHigh:4;
  unsigned m_Avl:1;
  unsigned m_Zero:1;
  unsigned m_BigDefault:1;
  unsigned m_Granularity:1;
  uint8_t m_BaseHigh;
}__attribute__((packed));

struct X86TSS {
  uint16_t m_PreviousTaskLink;
  uint16_t m_Reserved1;

  uint32_t m_ESP0;
  uint16_t m_SS0;
  uint16_t m_Reserved2;

  uint32_t m_ESP1;
  uint16_t m_SS1;
  uint16_t m_Reserved3;

  uint32_t m_ESP2;
  uint16_t m_SS2;
  uint16_t m_Reserved4;

  uint32_t m_CR3;
  uint32_t m_EIP;
  uint32_t m_EFLAGS;
  uint32_t m_EAX;
  uint32_t m_ECX;
  uint32_t m_EDX;
  uint32_t m_EBX;
  uint32_t m_ESP;
  uint32_t m_EBP;
  uint32_t m_ESI;
  uint32_t m_EDI;

  uint16_t m_ES;
  uint16_t m_Reserved5;
  uint16_t m_CS;
  uint16_t m_Reserved6;
  uint16_t m_SS;
  uint16_t m_Reserved7;
  uint16_t m_DS;
  uint16_t m_Reserved8;
  uint16_t m_FS;
  uint16_t m_Reserved9;
  uint16_t m_GS;
  uint16_t m_Reserved10;
  uint16_t m_LDT;
  uint16_t m_Reserved11;

  unsigned m_DebugTrap:1;
  unsigned m_Reserved12:15;
  uint16_t m_IOBase;

  void dumpInfo(llvm::raw_ostream &o);
}__attribute__((packed));

class X86Parser
{
public:
    typedef std::vector<X86IDTEntry> IDT;
    typedef std::vector<X86GDTEntry> GDT;

    static bool getIdt(S2EExecutionState *state, IDT &table);
    static bool getGdt(S2EExecutionState *state, GDT &Table);
    static bool getGdtEntry(S2EExecutionState *state, X86GDTEntry *gdtEntry, uint16_t selector);

    static bool getTss(S2EExecutionState *state, uint32_t base, X86TSS *tss);
    static bool getCurrentTss(S2EExecutionState *state, X86TSS *tss);

    static uint32_t getBase(const X86GDTEntry &E);
    static uint32_t getLimit(const X86GDTEntry &E);
    static uint32_t getOffset(const X86IDTEntry &E);
    static bool isTss(const X86GDTEntry &E);

};


class X86ExceptionInterceptor : public Plugin
{
    S2E_PLUGIN
public:
    struct Handler {
        uint64_t pc;
        EX86Exceptions idtVector;

        bool operator()(const Handler&h1, const Handler &h2) const {
            return h1.pc < h2.pc;
        }
    };

    typedef std::set<Handler, Handler> Handlers;

    X86ExceptionInterceptor(S2E* s2e): Plugin(s2e) {}

    void initialize();
    void onTranslateBlockStart(ExecutionSignal*, S2EExecutionState *state,
        TranslationBlock *tb, uint64_t pc);
    void onExecuteBlockStart(S2EExecutionState* state, uint64_t pc);

private:
    X86Parser::IDT m_idt;
    X86Parser::GDT m_gdt;
    bool m_inited;
    Handlers m_handlers;
    sigc::connection m_tbTranslateConnection;


    bool initializeExceptionHandlers(S2EExecutionState *state);
    bool registerHandler(S2EExecutionState *state, EX86Exceptions idtVector);
    void handleTaskGate(S2EExecutionState *state, const Handler &hdlr);
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_X86EI_H
