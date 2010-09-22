#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include "BlueScreenInterceptor.h"
#include <s2e/Plugins/ModuleExecutionDetector.h>

#include <iomanip>

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
    std::ostream &os = s2e()->getMessagesStream(state);

    os << "Killing state "  << state->getID() <<
            " because of BSOD " << std::endl;

    ModuleExecutionDetector *m_exec = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");

    if (m_exec) {
        m_exec->dumpMemory(state, os, state->getSp(), 512);
    }else {
        state->dumpStack(512);
    }

    s2e()->getExecutor()->terminateStateEarly(*state, "Killing because of BSOD");
}

}
}
