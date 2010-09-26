extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#define CURRENT_CLASS NtoskrnlHandlers

#include "NtoskrnlHandlers.h"

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>
#include <klee/Solver.h>

#include <iostream>
#include <sstream>

using namespace s2e::windows;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(NtoskrnlHandlers, "Basic collection of NT Kernel API functions.", "NtoskrnlHandlers",
                  "FunctionMonitor", "Interceptor");


void NtoskrnlHandlers::initialize()
{
    WindowsApi::initialize();

    m_loaded = false;

    m_windowsMonitor->onModuleLoad.connect(
            sigc::mem_fun(*this,
                    &NtoskrnlHandlers::onModuleLoad)
            );

    m_windowsMonitor->onModuleUnload.connect(
            sigc::mem_fun(*this,
                    &NtoskrnlHandlers::onModuleUnload)
            );

}

void NtoskrnlHandlers::onModuleLoad(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    if (module.Name != "ntoskrnl.exe") {
        return;
    }

    if (m_loaded) {
        return;
    }

    m_loaded = true;
    m_module = module;

    //Register the default set of functions
    //XXX: differentiate versions

    FunctionMonitor::CallSignal *cs;
    uint32_t dbgPrintAddr = m_module.LoadBase - 0x400000 + 0x427327;
    REGISTER_ENTRY_POINT(cs, dbgPrintAddr, DebugPrint);
}

void NtoskrnlHandlers::onModuleUnload(
    S2EExecutionState* state,
    const ModuleDescriptor &module
    )
{
    if (module.Name != "ntoskrnl.exe") {
        return;
    }

    //If we get here, Windows is broken.
    m_loaded = false;

    //XXX: Unregister all signals, but is it necessary?
}

void NtoskrnlHandlers::DebugPrint(S2EExecutionState* state, FunctionMonitorState *fns)
{
    //Invoke this function in all contexts
     uint32_t strptr;
     bool ok = true;
     ok &= readConcreteParameter(state, 1, &strptr);

     if (!ok) {
         s2e()->getDebugStream() << "Could not read string in DebugPrint" << std::endl;
         return;
     }

     std::string message;
     ok = state->readString(strptr, message, 255);
     if (!ok) {
         s2e()->getDebugStream() << "Could not read string in DebugPrint at address 0x" << std::hex << strptr <<  std::endl;
         return;
     }

     s2e()->getMessagesStream(state) << "DebugPrint: " << message << std::endl;
}



}
}
