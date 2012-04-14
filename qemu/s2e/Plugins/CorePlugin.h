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

#ifndef S2E_CORE_PLUGIN_H
#define S2E_CORE_PLUGIN_H

#include <s2e/Plugin.h>
#include <klee/Expr.h>

#include <s2e/Signals/Signals.h>
#include <vector>
#include <inttypes.h>

extern "C" {
typedef struct TranslationBlock TranslationBlock;
struct QEMUTimer;
}

namespace s2e {

class S2EExecutionState;

/** A type of a signal emitted on instruction execution. Instances of this signal
    will be dynamically created and destroyed on demand during translation. */
typedef sigc::signal<void, S2EExecutionState*, uint64_t /* pc */> ExecutionSignal;

/** This is a callback to check whether some port returns symbolic values.
  * An interested plugin can use it. Only one plugin can use it at a time.
  * This is necessary tp speedup checks (and avoid using signals) */
typedef bool (*SYMB_PORT_CHECK)(uint16_t port, void *opaque);
typedef bool (*SYMB_MMIO_CHECK)(uint64_t physaddress, uint64_t size, void *opaque);

class CorePlugin : public Plugin {
    S2E_PLUGIN

private:
    struct QEMUTimer *m_Timer;
    SYMB_PORT_CHECK m_isPortSymbolicCb;
    SYMB_MMIO_CHECK m_isMmioSymbolicCb;
    void *m_isPortSymbolicOpaque;
    void *m_isMmioSymbolicOpaque;

public:
    CorePlugin(S2E* s2e): Plugin(s2e) {
        m_Timer = NULL;
        m_isPortSymbolicCb = NULL;
        m_isMmioSymbolicCb = NULL;
        m_isPortSymbolicOpaque = NULL;
        m_isMmioSymbolicOpaque = NULL;
    }

    void initialize();
    void initializeTimers();

    void setPortCallback(SYMB_PORT_CHECK cb, void *opaque) {
        m_isPortSymbolicCb = cb;
        m_isPortSymbolicOpaque = opaque;
    }

    void setMmioCallback(SYMB_MMIO_CHECK cb, void *opaque) {
        m_isMmioSymbolicCb = cb;
        m_isMmioSymbolicOpaque = opaque;
    }

    inline bool isPortSymbolic(uint16_t port) const {
        if (m_isPortSymbolicCb) {
            return m_isPortSymbolicCb(port, m_isPortSymbolicOpaque);
        }
        return false;
    }

    inline bool isMmioSymbolic(uint64_t physAddress, uint64_t size) const {
        if (m_isMmioSymbolicCb) {
            return m_isMmioSymbolicCb(physAddress, size, m_isMmioSymbolicOpaque);
        }
        return false;
    }

    struct QEMUTimer *getTimer() {
        return m_Timer;
    }

    /** Signal that is emitted on begining and end of code generation
        for each QEMU translation block.
    */
    sigc::signal<void, ExecutionSignal*, 
            S2EExecutionState*,
            TranslationBlock*,
            uint64_t /* block PC */>
            onTranslateBlockStart;

    /** Signal that is emitted upon end of translation block */
    sigc::signal<void, ExecutionSignal*, 
            S2EExecutionState*,
            TranslationBlock*,
            uint64_t /* ending instruction pc */,
            bool /* static target is valid */,
            uint64_t /* static target pc */>
            onTranslateBlockEnd;

    
    /** Signal that is emitted on code generation for each instruction */
    sigc::signal<void, ExecutionSignal*,
            S2EExecutionState*,
            TranslationBlock*,
            uint64_t /* instruction PC */>
            onTranslateInstructionStart, onTranslateInstructionEnd;

    /**
     *  Triggered *after* each instruction is translated to notify
     *  plugins of which registers are used by the instruction.
     *  Each bit of the mask corresponds to one of the registers of
     *  the architecture (e.g., R_EAX, R_ECX, etc).
     */
    sigc::signal<void,
                 ExecutionSignal*,
                 S2EExecutionState* /* current state */,
                 TranslationBlock*,
                 uint64_t /* program counter of the instruction */,
                 uint64_t /* registers read by the instruction */,
                 uint64_t /* registers written by the instruction */,
                 bool /* instruction accesses memory */>
          onTranslateRegisterAccessEnd;

    /** Signal that is emitted on code generation for each jump instruction */
    sigc::signal<void, ExecutionSignal*,
            S2EExecutionState*,
            TranslationBlock*,
            uint64_t /* instruction PC */,
            int /* jump_type */>
            onTranslateJumpStart;

    /** Signal that is emitted upon exception */
    sigc::signal<void, S2EExecutionState*, 
            unsigned /* Exception Index */,
            uint64_t /* pc */>
            onException;

    /** Signal that is emitted when custom opcode is detected */
    sigc::signal<void, S2EExecutionState*, 
            uint64_t  /* arg */
            >
            onCustomInstruction;

    /** Signal that is emitted on each memory access */
    /* XXX: this signal is still not emmited for code */
    sigc::signal<void, S2EExecutionState*,
                 klee::ref<klee::Expr> /* virtualAddress */,
                 klee::ref<klee::Expr> /* hostAddress */,
                 klee::ref<klee::Expr> /* value */,
                 bool /* isWrite */, bool /* isIO */>
            onDataMemoryAccess;

    /** Signal that is emitted on each port access */
    sigc::signal<void, S2EExecutionState*,
                 klee::ref<klee::Expr> /* port */,
                 klee::ref<klee::Expr> /* value */,
                 bool /* isWrite */>
            onPortAccess;

    sigc::signal<void> onTimer;

    /** Signal emitted when the state is forked */
    sigc::signal<void, S2EExecutionState* /* originalState */,
                 const std::vector<S2EExecutionState*>& /* newStates */,
                 const std::vector<klee::ref<klee::Expr> >& /* newConditions */>
            onStateFork;

    sigc::signal<void,
                 S2EExecutionState*, /* currentState */
                 S2EExecutionState*> /* nextState */
            onStateSwitch;

    /** Signal emitted when spawning a new S2E process */
    sigc::signal<void, bool /* prefork */,
                bool /* ischild */,
                unsigned /* parentProcId */> onProcessFork;

    /**
     * Signal emitted when a new S2E process was spawned and all
     * parent states were removed from the child and child states
     * removed from the parent.
     */
    sigc::signal<void, bool /* isChild */> onProcessForkComplete;


    /** Signal that is emitted upon TLB miss */
    sigc::signal<void, S2EExecutionState*, uint64_t, bool> onTlbMiss;

    /** Signal that is emitted upon page fault */
    sigc::signal<void, S2EExecutionState*, uint64_t, bool> onPageFault;

    /** Signal emitted when QEMU is ready to accept registration of new devices */
    sigc::signal<void> onDeviceRegistration;

    /** Signal emitted when QEMU is ready to activate registered devices */
    sigc::signal<void, int /* bus type */,
                 void *> /* bus */
            onDeviceActivation;

    /**
     * The current execution privilege level was changed (e.g., kernel-mode=>user-mode)
     * previous and current are privilege levels. The meaning of the value may
     * depend on the architecture.
     */
    sigc::signal<void,
                 S2EExecutionState* /* current state */,
                 unsigned /* previous level */,
                 unsigned /* current level */>
          onPrivilegeChange;

    /**
     * The current page directory was changed.
     * This may occur, e.g., when the OS swaps address spaces.
     * The addresses correspond to physical addresses.
     */
    sigc::signal<void,
                 S2EExecutionState* /* current state */,
                 uint64_t /* previous page directory base */,
                 uint64_t /* current page directory base */>
          onPageDirectoryChange;

    /**
     * S2E completed initialization and is about to enter
     * the main execution loop for the first time.
     */
    sigc::signal<void,
                 S2EExecutionState* /* current state */>
          onInitializationComplete;
};

} // namespace s2e

#endif // S2E_CORE_PLUGIN_H
