#ifndef S2E_CORE_PLUGIN_H
#define S2E_CORE_PLUGIN_H

#include <s2e/Plugin.h>

#include <sigc++/sigc++.h>
#include <vector>
#include <inttypes.h>

extern "C" {
typedef struct TranslationBlock TranslationBlock;
}

namespace s2e {

class S2EExecutionState;

/** A type of a signal emited on instruction execution. Instances of this signal
    will be dynamically created and destroyed on demand during translation. */
typedef sigc::signal<void, S2EExecutionState*, uint64_t /* pc */> ExecutionSignal;

class CorePlugin : public Plugin {
    S2E_PLUGIN
public:
    CorePlugin(S2E* s2e): Plugin(s2e) {}

    void initialize() {}

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

    /** Signal that is emitted upon exception */
    sigc::signal<void, S2EExecutionState*, 
            unsigned /* Exception Index */,
            uint64_t /* pc */>
            onException;

    /** Signal that is emitted when custom opcode is detected */
    sigc::signal<void, S2EExecutionState*, 
            uint64_t  /* opcode */
            >
            onCustomInstruction;

    /** Signal that is emitted on each memory access */
    sigc::signal<void, S2EExecutionState*, uint64_t /* hostAddress */,
            uint8_t* /* buf */, uint64_t /* size */>
            onReadRam;

    sigc::signal<void, S2EExecutionState*, uint64_t /* hostAddress */,
            const uint8_t* /* buf */, uint64_t /* size */>
            onWriteRam;
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
