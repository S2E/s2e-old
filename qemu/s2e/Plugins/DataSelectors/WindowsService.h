#ifndef S2E_PLUGINS_WINDOWS_SERVICE_H
#define S2E_PLUGINS_WINDOWS_SERVICE_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include "DataSelector.h"
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>

namespace s2e {
namespace plugins {

struct WindowsServiceCfg
{
    std::string serviceId;
    std::string moduleId;
    bool makeParamCountSymbolic;
    bool makeParamsSymbolic;
};

class WindowsService : public DataSelector
{
    S2E_PLUGIN
public:
    WindowsService(S2E* s2e): DataSelector(s2e) {}

    void initialize();
    void onTranslateBlockStart(ExecutionSignal*, S2EExecutionState *state, 
        const ModuleDescriptor &desc,
        TranslationBlock *tb, uint64_t pc);
    
    void onExecution(S2EExecutionState *state, uint64_t pc);

private:
    //Handle only one service for now
    WindowsServiceCfg m_ServiceCfg;

    WindowsMonitor *m_WindowsMonitor;
    ModuleExecutionDetector *m_executionDetector;
    std::set<std::string> m_modules;
    sigc::connection m_TbConnection;

    virtual bool initSection(const std::string &cfgKey, const std::string &svcId);
    
    
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
