/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

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
#include "Ntddk.h"

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>
#include <klee/Solver.h>

#include <iostream>
#include <sstream>

using namespace s2e::windows;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(HalHandlers, "Basic collection of NT Hal API functions.", "HalHandlers",
                  "FunctionMonitor", "Interceptor");

const WindowsApiHandler<HalHandlers::EntryPoint> HalHandlers::s_handlers[] = {
    DECLARE_EP_STRUC(HalHandlers, HalpValidPciSlot),
};

const char * HalHandlers::s_ignoredFunctionsList[] = {
    NULL
};

const SymbolDescriptor HalHandlers::s_exportedVariablesList[] = {
    {"", 0}
};


const HalHandlers::HalHandlersMap HalHandlers::s_handlersMap =
        HalHandlers::initializeHandlerMap();

const HalHandlers::StringSet HalHandlers::s_ignoredFunctions =
        HalHandlers::initializeIgnoredFunctionSet();

const SymbolDescriptors HalHandlers::s_exportedVariables =
        HalHandlers::initializeExportedVariables();


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
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << '\n';

    uint32_t pBusHandler, slotNumber;
     bool ok = true;
     ok &= readConcreteParameter(state, 0, &pBusHandler);
     ok &= readConcreteParameter(state, 1, &slotNumber);

     if (!ok) {
         s2e()->getDebugStream() << "Could not read  in HalpValidPciSlot" << '\n';
         return;
     }

     BUS_HANDLER32 BusHandler;
     ok = state->readMemoryConcrete(pBusHandler, &BusHandler, sizeof(BusHandler));
     if (!ok) {
         s2e()->getDebugStream() << "Could not read BUS_HANDLER32 at address " << hexval(pBusHandler) <<  '\n';
         return;
     }

     BusHandler.print(s2e()->getMessagesStream(state));


}



}
}
