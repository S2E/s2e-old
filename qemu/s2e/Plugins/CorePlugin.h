#ifndef S2E_CORE_PLUGIN_H
#define S2E_CORE_PLUGIN_H

#include <s2e/Plugin.h>

#include <sigc++/sigc++.h>
#include <vector>
#include <inttypes.h>

namespace s2e {

/** A type of a signal emited on instruction execution. Instances of this signal
    will be dynamically created and destroyed on demand during translation. */
typedef sigc::signal<void, uint64_t /* pc */> ExecutionSignal;

class CorePlugin : public Plugin {
    S2E_PLUGIN
public:
    CorePlugin(S2E* s2e): Plugin(s2e) {}

    void initialize() {}

    /** Signal that is emitted on begining and end of code generation
        for each QEMU translation block.
    */
    sigc::signal<void, ExecutionSignal*, uint64_t /* block PC */>
            onTranslateBlockStart;

    /** Signal that is emitted on code generation for each instruction */
    sigc::signal<void, ExecutionSignal*, uint64_t /* instruction PC */>
            onTranslateInstructionStart, onTranslateInstructionEnd;

    /** Signals that we will need */
#if 0
    sigc::signal<execution_handler, uint64_t /* cr3 */, uint64_t /* startPC */, bool /* blockEntry */> onBlockExecution;
    sigc::signal<execution_handler, bool /* instrEntry */> onInstructionExecution;
    sigc::signal<execution_handler, uint64_t /* cr3 */, uint64_t /*callerPc*/, uint64_t /* calleePc */> onFunctionCall;
    sigc::signal<execution_handler, uint64_t /* cr3 */, uint64_t /*jumpPc*/, uint64_t /* targetPc */, JumpType > onJump;
    sigc::signal<execution_handler, ModuleDescriptor /* source */, ModuleDescriptor /* target */> onModuleTransition;
    sigc::signal<execution_handler, QEMUExecutionState /*parent*/, QEMUExecutionState /*new*/,
        uint64_t Pc, llvm::Instruction /*Instr*/> onFork;

    /** Triggered when a bug condition is detected (assert violation, blue screen) **/
    /* can be used to dump execution path in a file, etc. */
    sigc::signal<execution_handler, QEMUExecutionState /* state */, uint64_t Pc> onBug;
#endif
};

struct S2ETranslationBlock
{
    /** A list of all instruction execution signals associated with
        this basic block. All signals in the list will be deleted
        when this translation block will be flushed */
    std::vector<ExecutionSignal*> executionSignals;
};

} // namespace s2e

#endif // S2E_CORE_PLUGIN_H
