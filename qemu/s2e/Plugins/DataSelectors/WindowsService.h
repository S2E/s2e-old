#ifndef S2E_PLUGINS_WINDOWS_SERVICE_H
#define S2E_PLUGINS_WINDOWS_SERVICE_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include "DataSelector.h"
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>

namespace s2e {
namespace plugins {

class WindowsService : public DataSelector
{
    S2E_PLUGIN
public:
    WindowsService(S2E* s2e): DataSelector(s2e) {}

    void initialize();
    void onTranslateBlockStart(ExecutionSignal*, S2EExecutionState *state, 
        const ModuleExecutionDesc*desc,
        TranslationBlock *tb, uint64_t pc);
    void onExecution(S2EExecutionState* state, uint64_t pc);

private:
    std::set<std::string> m_Modules;
    sigc::connection m_TbConnection;

    bool initSection(const std::string &cfgKey);
    
    ModuleExecutionDetector *m_ExecDetector;
    WindowsMonitor *m_WindowsMonitor;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
