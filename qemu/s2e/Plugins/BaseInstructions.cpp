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

}

void BaseInstructions::onCustomInstruction(S2EExecutionState* state, 
        unsigned length, const uint8_t *code)
{
    TRACE("Custom instructions of length %d\n", length);
}

}
}