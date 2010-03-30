#ifndef S2E_CORE_PLUGIN_H
#define S2E_CORE_PLUGIN_H

#include <inttypes.h>

#ifdef __cplusplus

#include <s2e/Plugin.h>
#include <sigc++/sigc++.h>
#include <vector>

/* This is something like a clousure, but callable
   from generated code (so no member functions) */
struct ExecutionHandler {
    typedef void (*HandlerFunction)(Plugin* plugin, uint64_t arg, uint64_t pc);

    HandlerFunction function;
    Plugin*  plugin;
    uint64_t arg;

    ExecutionHandler(HandlerFunction _function, Plugin* _plugin, uint64_t _arg)
            : function(_function), plugin(_plugin), arg(_arg) {}
};

class CorePlugin : public Plugin {
    S2E_PLUGIN
public:
    CorePlugin(S2E* s2e): Plugin(s2e) {}

    void initialize() {}

    /** Signal that is emitted on begining and end of code generation
        for each QEMU translation block.
    */
    sigc::signal<void, std::vector<ExecutionHandler>*, uint64_t /* block PC */>
            onTranslateStart;

    /** Signal that is emitted on code generation for each instruction */
    sigc::signal<void, std::vector<ExecutionHandler>*, uint64_t /* instruction PC */>
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

#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif

/* Hooks for QEMU */

struct S2E;

void s2e_on_translate_start(struct S2E* s2e, uint64_t pc);
void s2e_on_translate_instruction_start(struct S2E* s2e, uint64_t pc);
void s2e_on_translate_instruction_end(struct S2E* s2e, uint64_t pc);

#ifdef __cplusplus
}
#endif

#endif // S2E_CORE_PLUGIN_H
