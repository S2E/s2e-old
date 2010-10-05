extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#define CURRENT_CLASS HalHandlers

#include "HalHandlers.h"

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>
#include <klee/Solver.h>

#include <iostream>
#include <sstream>

using namespace s2e::windows;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(HalHandlers, "Basic collection of NT Hal API functions.", "HalHandlers",
                  "FunctionMonitor", "Interceptor");


void HalHandlers::initialize()
{
    WindowsApi::initialize();

    m_loaded = false;

    m_windowsMonitor->onModuleLoad.connect(
            sigc::mem_fun(*this,
                    &HalHandlers::onModuleLoad)
            );

    m_windowsMonitor->onModuleUnload.connect(
            sigc::mem_fun(*this,
                    &HalHandlers::onModuleUnload)
            );

}

void HalHandlers::onModuleLoad(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    //XXX: check for kernel mode as well
    if (module.Name != "hal.dll") {
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
    uint32_t dbgPrintAddr = m_module.LoadBase - 0x80010000 + 0x80016020;
    REGISTER_ENTRY_POINT(cs, dbgPrintAddr, HalpValidPciSlot);
}

void HalHandlers::onModuleUnload(
    S2EExecutionState* state,
    const ModuleDescriptor &module
    )
{
    if (module.Name != "hal.dll") {
        return;
    }

    //If we get here, Windows is broken.
    m_loaded = false;

    //XXX: Unregister all signals, but is it necessary?
}


//BOOLEAN HalpValidPCISlot(IN PBUS_HANDLER BusHandler, IN PCI_SLOT_NUMBER Slot)
void HalHandlers::HalpValidPciSlot(S2EExecutionState* state, FunctionMonitorState *fns)
{
    //Invoke this function in all contexts
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    uint32_t pBusHandler, slotNumber;
     bool ok = true;
     ok &= readConcreteParameter(state, 0, &pBusHandler);
     ok &= readConcreteParameter(state, 1, &slotNumber);

     if (!ok) {
         s2e()->getDebugStream() << "Could not read  in HalpValidPciSlot" << std::endl;
         return;
     }

     BUS_HANDLER32 BusHandler;
     ok = state->readMemoryConcrete(pBusHandler, &BusHandler, sizeof(BusHandler));
     if (!ok) {
         s2e()->getDebugStream() << "Could not read BUS_HANDLER32 at address 0x" << std::hex << pBusHandler <<  std::endl;
         return;
     }

     std::ostream &os = s2e()->getMessagesStream(state);
     BusHandler.print(os);


}



}
}
