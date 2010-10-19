#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/S2EExecutor.h>
#include "BlueScreenInterceptor.h"
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsApi/Api.h>

#include <iomanip>
#include <sstream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(BlueScreenInterceptor, "Intercepts Windows blue screens of death and generated bug reports",
                  "BlueScreenInterceptor", "WindowsMonitor");

void BlueScreenInterceptor::initialize()
{
    m_monitor = static_cast<WindowsMonitor*>(s2e()->getPlugin("WindowsMonitor"));
    assert(m_monitor);

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
            sigc::mem_fun(*this,
                    &BlueScreenInterceptor::onTranslateBlockStart));

    bool ok;
    m_generateCrashDump = s2e()->getConfig()->getBool(getConfigKey() + ".generateCrashDump", false, &ok);
    if (m_generateCrashDump) {
        m_crashdumper = static_cast<WindowsCrashDumpGenerator*>(s2e()->getPlugin("WindowsCrashDumpGenerator"));
        if (!m_crashdumper) {
            s2e()->getWarningsStream() << "The WindowsCrashDumpGenerator plugin is required with the " <<
                    "generateCrashDump option." << std::endl;
            exit(-1);
        }

        //How many dumps to generate at most ?
        //Dumps are large and there can be many of them.
        m_currentDumpCount = 0;
        m_maxDumpCount = s2e()->getConfig()->getInt(getConfigKey() + ".maxDumpCount", (int64_t)-1, &ok);
        s2e()->getDebugStream() << "BlueScreenInterceptor: Maximum number of dumps:" << m_maxDumpCount << std::endl;
    }
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

    dispatchErrorCodes(state);

    if (m_exec) {
        m_exec->dumpMemory(state, os, state->getSp(), 512);
    }else {
        state->dumpStack(512);
    }

    if (m_generateCrashDump && m_currentDumpCount < m_maxDumpCount) {
        ++m_currentDumpCount;
        m_crashdumper->generateDumpOnBsod(state, "bsod");
    }

    s2e()->getExecutor()->terminateStateEarly(*state, "Killing because of BSOD");
}

void BlueScreenInterceptor::dumpCriticalObjectTermination(S2EExecutionState *state)
{
    uint32_t terminatingObjectType;
    uint32_t terminatingObject;
    uint32_t processImageName;
    uint32_t message;

    bool ok = true;
    ok &= WindowsApi::readConcreteParameter(state, 1, &terminatingObjectType);
    ok &= WindowsApi::readConcreteParameter(state, 2, &terminatingObject);
    ok &= WindowsApi::readConcreteParameter(state, 3, &processImageName);
    ok &= WindowsApi::readConcreteParameter(state, 4, &message);

    if (!ok) {
        s2e()->getDebugStream() << "Could not read BSOD parameters" << std::endl;
    }

    std::string strMessage, strImage;
    ok &= state->readString(message, strMessage, 256);
    ok &= state->readString(processImageName, strImage, 256);

    s2e()->getDebugStream(state) <<
            "CRITICAL_OBJECT_TERMINATION" << std::endl <<
            "ImageName: " << strImage << std::endl <<
            "Message:   " << strMessage << std::endl;
}

void BlueScreenInterceptor::dispatchErrorCodes(S2EExecutionState *state)
{
    uint32_t errorCode;

    WindowsApi::readConcreteParameter(state, 0, &errorCode);
    switch(errorCode) {
    case CRITICAL_OBJECT_TERMINATION:
        dumpCriticalObjectTermination(state);
        break;

    default:
        s2e()->getDebugStream() << "Unknown BSOD code " << errorCode << std::endl;
        break;
    }
}


}
}
