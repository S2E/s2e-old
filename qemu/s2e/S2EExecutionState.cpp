extern "C" {
#include "config.h"
#include "qemu-common.h"
}


#include "S2EExecutionState.h"
#include <s2e/s2e_qemu.h>

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

/******************************/
/* Functions called from QEMU */

extern "C" {

S2EExecutionState* g_s2e_state = NULL;

void s2e_update_state_env(
        struct S2EExecutionState* state, CPUX86State* env)
{
	state->cpuState = env;
}

void s2e_update_state_env_pc(
        struct S2EExecutionState* state, CPUX86State* env, uint64_t pc)
{
    state->cpuState = env;
    state->cpuPC = pc;
}

} // extern "C"
