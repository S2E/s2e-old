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

    uint64_t sp = state->getSp();
    for (unsigned i=0; i<512; ++i) {
        klee::ref<klee::Expr> val = state->readMemory(sp + i * sizeof(uint32_t), klee::Expr::Int32);
        klee::ConstantExpr *ce = dyn_cast<klee::ConstantExpr>(val);
        if (ce) {
            os << std::hex << "0x" << sp + i * sizeof(uint32_t) << " 0x" << std::setw(sizeof(uint32_t)*2) << std::setfill('0') << val;
            os << std::setfill(' ');

            if (m_exec) {
                uint32_t v = ce->getZExtValue(ce->getWidth());
                const ModuleDescriptor *md = m_exec->getModule(state,  v, false);
                if (md) {
                    os << " " << md->Name <<  " 0x" << md->ToNativeBase(v);
                    os << " +0x" << md->ToRelative(v);
                }
            }
        }else {
            os << std::hex << "0x" << sp + i * sizeof(uint32_t) << val;
        }

        os << std::endl;
    }



    s2e()->getExecutor()->terminateStateOnExit(*state);
}

}
}
