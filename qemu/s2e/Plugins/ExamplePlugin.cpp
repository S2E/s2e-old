#include "ExamplePlugin.h"
#include <s2e/S2E.h>

#include <iostream>

S2E_DEFINE_PLUGIN(ExamplePlugin, "Example S2E plugin");

void ExamplePlugin::initialize()
{
    s2e()->getCorePlugin()->onTranslateStart.connect(
            sigc::mem_fun(*this, &ExamplePlugin::slotTranslateBlock));
}

void ExamplePlugin::slotTranslateBlock(std::vector<ExecutionHandler>*, uint64_t pc)
{
    std::cout << "Translating block at " << std::hex << pc << std::dec << std::endl;
}
