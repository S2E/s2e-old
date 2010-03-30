#ifndef S2E_CORE_PLUGIN_H*
#define S2E_CORE_PLUGIN_H

struct CPUState;
typedef void (*execution_handler)(CPUState*);

#include <inttypes.h>

#ifdef __cplusplus

#include <s2e/Plugin.h>
#include <sigc++/sigc++.h>

class CorePlugin : public Plugin {
    S2E_PLUGIN
public:
    void initialize() {}

    /** Signal that is emitted on begining and end of code generation
        for each QEMU translation block.

        Returns a handler to insert, or NULL.
        TODO: accomulators to collect all handlers
    */
    sigc::signal<execution_handler,
        uint64_t /* cr3 */, uint64_t /* startPC */, uint64_t /* endPC */>
            onTBTranslateStart, onTBTranslateEnd;

    /** Signal that is emitted on code generation for each instruction */
    sigc::signal<execution_handler,
        uint64_t /* cr3 */, uint64_t /* PC */>
            onTBTranslateInstruction;

    /** Signals that we will need */
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
    
};

#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif

/* Hooks for QEMU */

execution_handler s2e_tb_translate_start(
        uint64_t cr3, uint64_t start_pc, uing64_t end_pc);

execution_handler s2e_tb_translate_end(
        uint64_t cr3, uint64_t start_pc, uing64_t end_pc);

execution_handler s2e_tb_translate_instruction(
        uint64_t cr3, uint64_t pc);

#ifdef __cplusplus
}
#endif

#endif // S2E_CORE_PLUGIN_H
