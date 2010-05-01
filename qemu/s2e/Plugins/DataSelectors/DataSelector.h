#ifndef S2E_PLUGINS_DATA_SELECTOR_H
#define S2E_PLUGINS_DATA_SELECTOR_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {

//This class contains generic methods to parse
//the constraints specified in the configuration files.
//Subclasses should implement platform-specific selector.
class DataSelector : public Plugin
{
public:
    DataSelector(S2E* s2e): Plugin(s2e) {}
    
private:
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
