#include "BaseInstructions.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(BaseInstructions, "Default set of custom instructions plugin", "",);

void BaseInstructions::initialize()
{
    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &BaseInstructions::onCustomInstruction));
}

void BaseInstructions::handleBuiltInOps(S2EExecutionState* state, 
        uint64_t opcode)
{
    switch(opcode & 0xFF) {
        case 1: state->enableSymbExec(); break;
        case 2: state->disableSymbExec(); break;
        case 3:
            {
                opcode >>= 8;
                std::cout << "Making register " << std::dec << (opcode & 0xFF)
                    << " of size " << ((opcode>>8) & 0xFF) << " symbolic" << std::endl;
            }
            break;
        default:
            std::cout << "Invalid built-in opcode 0x"<< std::hex << opcode << std::endl;
            break;
    }
}

void BaseInstructions::onCustomInstruction(S2EExecutionState* state, 
        uint64_t opcode)
{
    TRACE("Custom instructions %#"PRIx64"\n", opcode);

    switch(opcode & 0xFF) {
        case 0x00:
            handleBuiltInOps(state, opcode>>8);
            break;
        default:
            std::cout << "Invalid custom operation 0x"<< std::hex << opcode<< " at 0x" << 
                state->getPc() << std::endl;
    }
}

}
}