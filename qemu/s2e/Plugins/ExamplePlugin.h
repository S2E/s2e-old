#ifndef S2E_EXAMPLE_PLUGIN_H
#define S2E_EXAMPLE_PLUGIN_H

#include <s2e/Plugin.h>
#include <s2e/CorePlugin.h>

class ExamplePlugin : public Plugin, public sigc::trackable
{
    S2E_PLUGIN
public:
    ExamplePlugin(S2E* s2e): Plugin(s2e) {}

    void initialize();
    void slotTranslateBlock(std::vector<ExecutionHandler>*, uint64_t pc);
};

#endif // S2E_EXAMPLE_PLUGIN_H
