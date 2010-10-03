#ifndef S2E_CORE_PLUGIN_H
#define S2E_CORE_PLUGIN_H

#include <s2e/Plugin.h>
#include <klee/Expr.h>

#include <sigc++/sigc++.h>
#include <vector>
#include <inttypes.h>

extern "C" {
typedef struct TranslationBlock TranslationBlock;
struct QEMUTimer;
}

namespace s2e {

class S2EExecutionState;

/** A type of a signal emited on instruction execution. Instances of this signal
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

    /** Signal emited when the state is forked */
    sigc::signal<void, S2EExecutionState* /* originalState */,
                 const std::vector<S2EExecutionState*>& /* newStates */,
                 const std::vector<klee::ref<klee::Expr> >& /* newConditions */>
            onStateFork;

    /** Signal that is emitted upon TLB miss */
    sigc::signal<void, S2EExecutionState*, uint64_t, bool> onTlbMiss;

    /** Signal that is emitted upon page fault */
    sigc::signal<void, S2EExecutionState*, uint64_t, bool> onPageFault;

    /** Signal emitted when QEMU is ready to accept registration of new devices */
    sigc::signal<void> onDeviceRegistration;

    /** Signal emitted when QEMU is ready to activate registered devices */
    sigc::signal<void, struct PCIBus*> onDeviceActivation;

};

} // namespace s2e

#endif // S2E_CORE_PLUGIN_H
