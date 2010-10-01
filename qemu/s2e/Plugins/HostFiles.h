#ifndef S2E_PLUGINS_HOSTFILES_H
#define S2E_PLUGINS_HOSTFILES_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {

class HostFiles : public Plugin
{
    S2E_PLUGIN
public:
    HostFiles(S2E* s2e): Plugin(s2e) {}

    void initialize();

private:
    //bool m_allowWrite;
    std::string m_baseDir;
    std::vector<int> m_openFiles;

    void onCustomInstruction(S2EExecutionState* state, uint64_t opcode);
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_HOSTFILES_H
