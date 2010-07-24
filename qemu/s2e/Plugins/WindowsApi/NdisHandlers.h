#ifndef S2E_PLUGINS_NDISHANDLERS_H
#define S2E_PLUGINS_NDISHANDLERS_H

#include <s2e/Plugin.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>

namespace s2e {
namespace plugins {


class NdisHandlers : public Plugin
{
    S2E_PLUGIN
public:
    typedef std::set<std::string> StringSet;

    NdisHandlers(S2E* s2e): Plugin(s2e) {}

    void initialize();


private:
    FunctionMonitor *m_functionMonitor;
    WindowsMonitor *m_windowsMonitor;
    ModuleExecutionDetector *m_detector;

    //Modules we want to intercept
    StringSet m_modules;

    void onModuleLoad(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void entryPointCall(S2EExecutionState *state, FunctionMonitor::ReturnSignal *signal);
    void entryPointRet(S2EExecutionState *state);
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
