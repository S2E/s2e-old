#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include "BlueScreenInterceptor.h"


namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(BlueScreenInterceptor, "Intercepts Windows blue screens of death and generated bug reports",
                  "BlueScreenInterceptor", "WindowsMonitor");

void BlueScreenInterceptor::initialize()
{
    m_monitor = (WindowsMonitor*)s2e()->getPlugin("WindowsMonitor");
    assert(m_monitor);

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
            sigc::mem_fun(*this,
                    &BlueScreenInterceptor::onTranslateBlockStart));
}

void BlueScreenInterceptor::onTranslateBlockStart(
    ExecutionSignal *signal,
    S2EExecutionState *state,
    TranslationBlock *tb,
    uint64_t pc)
{
    if (!m_monitor->CheckPanic(pc)) {
        return;
    }

    signal->connect(sigc::mem_fun(*this,
        &BlueScreenInterceptor::onBsod));
}

void BlueScreenInterceptor::onBsod(
        S2EExecutionState *state, uint64_t pc)
{
    s2e()->getMessagesStream(state) << "Killing state "  << state->getID() <<
            " because of BSOD " << std::endl;
    s2e()->getExecutor()->terminateStateOnExit(*state);
}

}
}
