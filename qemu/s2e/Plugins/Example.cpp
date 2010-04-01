#include "Example.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(Example, "Example S2E plugin", "Core",);

void Example::initialize()
{
    m_traceBlockTranslation = s2e()->getConfig()->getBool(
                        getConfigKey() + ".traceBlockTranslation");
    m_traceBlockExecution = s2e()->getConfig()->getBool(
                        getConfigKey() + ".traceBlockExecution");

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
            sigc::mem_fun(*this, &Example::slotTranslateBlockStart));
}

void Example::slotTranslateBlockStart(ExecutionSignal *signal, uint64_t pc)
{
    if(m_traceBlockTranslation)
        std::cout << "Translating block at " << std::hex << pc << std::dec << std::endl;
    if(m_traceBlockExecution)
        signal->connect(sigc::mem_fun(*this, &Example::slotExecuteBlockStart));
}

void Example::slotExecuteBlockStart(S2EExecutionState *state, uint64_t pc)
{
    std::cout << "Executing block at " << std::hex << pc << std::dec << std::endl;
}

} // namespace plugins
} // namespace s2e
