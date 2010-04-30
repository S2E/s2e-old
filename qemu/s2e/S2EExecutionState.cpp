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

TranslationBlock *S2EExecutionState::getTb() const
{
    return cpuState->s2e_current_tb; 
}

uint64_t S2EExecutionState::getPid() const
{ 
    return cpuState->cr[3];
}

void S2EExecutionState::disableSymbExec() 
{
    printf("DISABLE symbexec at %#"PRIx64"\n", getPc());
}

void S2EExecutionState::enableSymbExec()
{
    printf("ENABLE symbexec at %#"PRIx64"\n", getPc());
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
