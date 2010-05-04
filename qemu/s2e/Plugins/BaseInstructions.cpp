#include "BaseInstructions.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/S2EExecutionState.h>
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
        uint64_t opcode, uint64_t value1)
{
    switch(opcode & 0xFF) {
        case 1: s2e()->getExecutor()->enableSymbolicExecution(state); break;
        case 2: s2e()->getExecutor()->disableSymbolicExecution(state); break;
        case 3: {
            int width = (opcode >> 8) & 0xff;
            if(width != 1 && width != 8 && width != 16
                    && width != 32 && width != 64) {
                s2e()->getWarningsStream()
                        << "Guest requested insertion of symbolic value"
                        << " of incorrect width " << width << std::endl;
            } else {
                s2e()->getMessagesStream()
                        << "Inserting symbolic value of width " << width
                        << " at " << hexval(value1) << std::endl;
                if(!state->writeMemory(value1,
                                       state->createSymbolicValue(width))) {
                    s2e()->getWarningsStream()
                        << "Can not insert symbolic value of width " << width
                        << " at " << hexval(value1)
                        << ": can not write to memory" << std::endl;
                }
            }
            break;
        }
        default:
            s2e()->getWarningsStream()
                << "Invalid built-in opcode " << hexval(opcode) << std::endl;
            break;
    }
}

void BaseInstructions::onCustomInstruction(S2EExecutionState* state, 
        uint64_t opcode, uint64_t value1)
{
    TRACE("Custom instructions %#"PRIx64" %#"PRIx64"\n", opcode, value1);

    switch(opcode & 0xFF) {
        case 0x00:
            handleBuiltInOps(state, opcode>>8, value1);
            break;
        default:
            std::cout << "Invalid custom operation 0x"<< std::hex << opcode<< " at 0x" << 
                state->getPc() << std::endl;
    }
}

}
}
