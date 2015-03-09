#ifndef S2E_PLUGINS_INPUTGENERATOR_H
#define S2E_PLUGINS_INPUTGENERATOR_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {

class InputGenerator : public Plugin
{
    S2E_PLUGIN
public:
    InputGenerator(S2E *s2e): Plugin(s2e) {}

    void initialize();
};

} // namespace plugins
} // namespace s2e

#endif
