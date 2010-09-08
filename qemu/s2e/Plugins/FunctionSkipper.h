#ifndef S2E_PLUGINS_FUNCSKIPPER_H
#define S2E_PLUGINS_FUNCSKIPPER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/OSMonitor.h>


namespace s2e {
namespace plugins {

    struct FunctionSkipperCfgEntry
    {
        std::string cfgname;
        std::string module;
        uint64_t address;
        bool isActive;
        bool executeOnce;
        bool symbolicReturn;
        unsigned paramCount, keepReturnPathsCount;
        std::vector<FunctionSkipperCfgEntry*> activateOnEntry;

        unsigned invocationCount, returnCount;

        FunctionSkipperCfgEntry() {
            keepReturnPathsCount = 0;
            invocationCount = returnCount = 0;
            address = 0;
            paramCount = 0;
            isActive = executeOnce = symbolicReturn = false;
        }
    };

class FunctionSkipper : public Plugin
{
    S2E_PLUGIN
public:
    typedef std::vector<FunctionSkipperCfgEntry*> CfgEntries;

    FunctionSkipper(S2E* s2e): Plugin(s2e) {}
    virtual ~FunctionSkipper();
    void initialize();

private:
    FunctionMonitor *m_functionMonitor;
    ModuleExecutionDetector *m_moduleExecutionDetector;
    OSMonitor *m_osMonitor;
    CfgEntries m_entries;

    bool initSection(const std::string &entry, const std::string &cfgname);
    bool resolveDependencies(const std::string &entry, FunctionSkipperCfgEntry *e);

    void onModuleLoad(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void onFunctionRet(
            S2EExecutionState* state,
            FunctionSkipperCfgEntry *entry
            );

    void onFunctionCall(
            S2EExecutionState* state,
            FunctionMonitorState *fns,
            FunctionSkipperCfgEntry *entry
            );
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_FUNCSKIPPER_H
