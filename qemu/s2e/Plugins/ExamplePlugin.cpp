#include "ExamplePlugin.h"
#include <s2e/S2E.h>

#include <iostream>

S2E_DEFINE_PLUGIN(ExamplePlugin, "Example S2E plugin");

void ExamplePlugin::initialize()
{
    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
            sigc::mem_fun(*this, &ExamplePlugin::slotTranslateBlockStart));
}

void ExamplePlugin::slotTranslateBlockStart(ExecutionSignal *signal, uint64_t pc)
{
    std::cout << "Translating block at " << std::hex << pc << std::dec << std::endl;
    signal->connect(sigc::mem_fun(*this, &ExamplePlugin::slotExecuteBlockStart));
}

void ExamplePlugin::slotExecuteBlockStart(uint64_t pc)
{
    std::cout << "Executing block at " << std::hex << pc << std::dec << std::endl;
}
