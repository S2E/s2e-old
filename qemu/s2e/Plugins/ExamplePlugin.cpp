#include "ExamplePlugin.h"
#include <s2e/S2E.h>

#include <iostream>

S2E_DEFINE_PLUGIN(ExamplePlugin, "Example S2E plugin");

void ExamplePlugin::initialize()
{
    m_traceBlockTranslation = s2e()->getConfig()->getBool(
                        getConfigKey() + ".traceBlockTranslation");
    m_traceBlockExecution = s2e()->getConfig()->getBool(
                        getConfigKey() + ".traceBlockExecution");

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
            sigc::mem_fun(*this, &ExamplePlugin::slotTranslateBlockStart));
}

void ExamplePlugin::slotTranslateBlockStart(ExecutionSignal *signal, uint64_t pc)
{
    if(m_traceBlockTranslation)
        std::cout << "Translating block at " << std::hex << pc << std::dec << std::endl;
    if(m_traceBlockExecution)
        signal->connect(sigc::mem_fun(*this, &ExamplePlugin::slotExecuteBlockStart));
}

void ExamplePlugin::slotExecuteBlockStart(uint64_t pc)
{
    std::cout << "Executing block at " << std::hex << pc << std::dec << std::endl;
}
