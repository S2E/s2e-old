extern "C" {
#include "config.h"
#include "qemu-common.h"
}


#include "S2EExecutionState.h"

namespace s2e {

using namespace klee;

//Get the program counter in the current state.
//Allows plugins to retrieve it in a hardware-independent manner.
uint64_t S2EExecutionState::getPc() const
{ 
    return cpuState->eip;
}

void S2EExecutionState::disableSymbExec() 
{
    std::cout << "DISABLE symbexec at 0x" << std::hex << getPc() << std::endl;
}

void S2EExecutionState::enableSymbExec()
{
    std::cout << "ENABLE  symbexec at 0x" << std::hex << getPc() << std::endl;
}



} // namespace s2e
