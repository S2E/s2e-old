#ifndef S2E_PLUGINS_NTOSKRNLHANDLERS_H
#define S2E_PLUGINS_NTOSKRNLHANDLERS_H

#include <s2e/Plugin.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>
#include <s2e/Plugins/SymbolicHardware.h>

#define CURRENT_CLASS NtoskrnlHandlers
#include "Api.h"

namespace s2e {
namespace plugins {

class NtoskrnlHandlers: public WindowsApi
{
    S2E_PLUGIN
public:
    NtoskrnlHandlers(S2E* s2e): WindowsApi(s2e) {}

    void initialize();

private:
    bool m_loaded;
    ModuleDescriptor m_module;

    void onModuleLoad(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void onModuleUnload(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        );

    DECLARE_ENTRY_POINT(DebugPrint);

};

}
}

#endif
