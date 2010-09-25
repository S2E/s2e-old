#ifndef S2E_PLUGINS_BSOD_H
#define S2E_PLUGINS_BSOD_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include "WindowsMonitor.h"

namespace s2e {
namespace plugins {

enum BsodCodes
{
    CRITICAL_OBJECT_TERMINATION = 0xf4
};

class BlueScreenInterceptor : public Plugin
{
    S2E_PLUGIN
public:
    BlueScreenInterceptor(S2E* s2e): Plugin(s2e) {}

    void initialize();

private:
    WindowsMonitor *m_monitor;

    void onTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t pc);

    void dumpCriticalObjectTermination(S2EExecutionState *state);
    void dispatchErrorCodes(S2EExecutionState *state);

    void onBsod(S2EExecutionState *state, uint64_t pc);
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
