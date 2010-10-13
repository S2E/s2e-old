#ifndef S2E_PLUGINS_CUSTINST_H

#define S2E_PLUGINS_CUSTINST_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {

class BaseInstructions : public Plugin
{
    S2E_PLUGIN
public:
    BaseInstructions(S2E* s2e): Plugin(s2e) {}

    void initialize();
   
    void handleBuiltInOps(S2EExecutionState* state, 
        uint64_t opcode);

private:
    void onCustomInstruction(S2EExecutionState* state, 
        uint64_t opcode);

    bool createTables();
    bool insertTiming(S2EExecutionState *state, uint64_t id);

};

} // namespace plugins
} // namespace s2e

#endif
