extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include "NdisHandlers.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/StateManager.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>

#include <iostream>

using namespace s2e::windows;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(NdisHandlers, "Basic collection of NDIS API functions.", "NdisHandlers",
                  "FunctionMonitor", "WindowsMonitor", "ModuleExecutionDetector");

void NdisHandlers::initialize()
{

    ConfigFile *cfg = s2e()->getConfig();

    m_functionMonitor = static_cast<FunctionMonitor*>(s2e()->getPlugin("FunctionMonitor"));
    m_windowsMonitor = static_cast<WindowsMonitor*>(s2e()->getPlugin("WindowsMonitor"));
    m_detector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));

    ConfigFile::string_list mods = cfg->getStringList(getConfigKey() + ".moduleIds");
    if (mods.size() == 0) {
        s2e()->getWarningsStream() << "No modules to track configured for the NdisHandlers plugin" << std::endl;
        return;
    }


    foreach2(it, mods.begin(), mods.end()) {
        m_modules.insert(*it);
    }

    m_windowsMonitor->onModuleLoad.connect(
            sigc::mem_fun(*this,
                    &NdisHandlers::onModuleLoad)
            );


}


void NdisHandlers::onModuleLoad(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    const std::string *s = m_detector->getModuleId(module);
    if (!s || (m_modules.find(*s) == m_modules.end())) {
        //Not the right module we want to intercept
        return;
    }

    //We loaded the module, instrument the entry point
    if (!module.EntryPoint) {
        s2e()->getWarningsStream() << "NdisHandlers: Module has no entry point ";
        module.Print(s2e()->getWarningsStream());
    }

    FunctionMonitor::CallSignal* entryPoint =
            m_functionMonitor->getCallSignal(module.ToRuntime(module.EntryPoint), 0);

    entryPoint->connect(sigc::mem_fun(*this, &NdisHandlers::entryPointCall));

    Imports I;
    if (!m_windowsMonitor->getImports(state, module, I)) {
        s2e()->getWarningsStream() << "NdisHandlers: Could not read imports for module ";
        module.Print(s2e()->getWarningsStream());
        return;
    }


}

void NdisHandlers::entryPointCall(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling NDIS entry point "
                << " at " << hexval(state->getPc()) << std::endl;

    signal->connect(sigc::mem_fun(*this, &NdisHandlers::entryPointRet));

}

void NdisHandlers::entryPointRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from NDIS entry point "
                << " at " << hexval(state->getPc()) << std::endl;

    //Check the success status
    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &eax, sizeof(eax)*8)) {
        s2e()->getWarningsStream() << "NdisHandlers::entryPointRet: could not read return status" << std::endl;
    }

    if (!NT_SUCCESS(eax)) {
        s2e()->getMessagesStream(state) << "Killing state "  << state->getID() <<
                " because EntryPoint failed with 0x" << std::hex << eax << std::endl;
        s2e()->getExecutor()->terminateStateOnExit(*state);
    }

    StateManager *mgr = StateManager::getManager(s2e());
    mgr->succeededState(state);

    if (mgr->empty()) {
        mgr->killAllButOneSuccessful();
    }
}

} // namespace plugins
} // namespace s2e
