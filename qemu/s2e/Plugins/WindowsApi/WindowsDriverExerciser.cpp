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


#include <sstream>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>
#include <s2e/S2EExecutor.h>

#include "WindowsDriverExerciser.h"
#include "NtoskrnlHandlers.h"
#include "Ntddk.h"

using namespace s2e::windows;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(WindowsDriverExerciser, "Windows driver tester",
                  "WindowsDriverExerciser", "Interceptor", "ModuleExecutionDetector");

void WindowsDriverExerciser::initialize()
{
    WindowsApi::initialize();


    ConfigFile *cfg = s2e()->getConfig();
    ConfigFile::string_list mods = cfg->getStringList(getConfigKey() + ".moduleIds");
    if (mods.size() == 0) {
        s2e()->getWarningsStream() << "WindowsDriverExerciser: You must specify modules to track in moduleIds" << '\n';
        exit(-1);
        return;
    }

    bool ok;
    std::string strUnloadAction = cfg->getString(getConfigKey() + ".unloadAction", "", &ok);

    if (strUnloadAction == "kill") {
        m_unloadAction = KILL;
    }else if (strUnloadAction == "succeed") {
        m_unloadAction = SUCCEED;
    }else {
        s2e()->getWarningsStream() << "WindowsDriverExerciser: You must specify kill or succeed for unloadAction" << '\n';
        exit(-1);
    }

    foreach2(it, mods.begin(), mods.end()) {
        m_modules.insert(*it);
    }

    m_windowsMonitor->onModuleLoad.connect(
            sigc::mem_fun(*this,
                    &WindowsDriverExerciser::onModuleLoad)
            );

    m_windowsMonitor->onModuleUnload.connect(
            sigc::mem_fun(*this,
                    &WindowsDriverExerciser::onModuleUnload)
            );


    ASSERT_STRUC_SIZE(ETHREAD32, ETHREAD32_SIZE)
}

void WindowsDriverExerciser::onModuleLoad(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    const std::string *s = m_detector->getModuleId(module);
    if (!s || (m_modules.find(*s) == m_modules.end())) {
        //Not the right module we want to intercept
        return;
    }

    //Skip those that were already loaded
    if (m_loadedModules.find(*s) != m_loadedModules.end()) {
        return;
    }

    m_loadedModules.insert(*s);

    //We loaded the module, instrument the entry point
    if (!module.EntryPoint) {
        s2e()->getWarningsStream() << "WindowsDriverExerciser: Module has no entry point ";
        module.Print(s2e()->getWarningsStream());
        return;
    }

    WINDRV_REGISTER_ENTRY_POINT(module.ToRuntime(module.EntryPoint), DriverEntryPoint);

    WindowsApi::registerImports(state, module);

    if (m_memoryChecker) {
        m_memoryChecker->grantMemoryForModuleSections(state, module);
    }

#if 0
    if (m_statsCollector) {
        //XXX: this should go into ModuleExecutionDetector
        m_statsCollector->incrementModuleLoads(state);
        if (m_statsCollector->getStatistics(state).moduleLoads > 4) {
            s2e()->getExecutor()->terminateStateEarly(*state, "Barrier");
        }
    }
#endif

    state->enableSymbolicExecution();
    state->enableForking();
}

void WindowsDriverExerciser::onModuleUnload(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    const std::string *s = m_detector->getModuleId(module);
    if (!s || (m_modules.find(*s) == m_modules.end())) {
        //Not the right module we want to intercept
        return;
    }

    m_functionMonitor->disconnect(state, module);
    unregisterEntryPoints(state, module);
    m_loadedModules.erase(*s);

    detectLeaks(state, module);

    //XXX: We might want to monitor multiple modules, so avoid killing
    switch(m_unloadAction) {
        case SUCCEED:
        if (m_manager) {
            m_manager->succeedState(state);
        }

        break;
        case KILL: {
            std::stringstream ss;
            ss << module.Name << " unloaded";
            s2e()->getExecutor()->terminateStateEarly(*state, ss.str());
        }
    }

    return;
}

void WindowsDriverExerciser::DriverEntryPoint(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();

    bool ok = true;
    uint32_t driverObject, registryPath;
    ok &= readConcreteParameter(state, 0, &driverObject);
    ok &= readConcreteParameter(state, 1, &registryPath);
    if (!ok) {
        s2e()->getWarningsStream() << "WindowsDriverExerciser: could not read driverObject and/or registryPath" << '\n';
        return;
    }

    if(m_memoryChecker) {
        //Entry point args
        //XXX: Fix the hard-coded sizes!
        m_memoryChecker->grantMemoryForModule(state, driverObject, 0x1000, MemoryChecker::READWRITE,
                                     "args:EntryPoint:DriverObject");
        m_memoryChecker->grantMemoryForModule(state, registryPath, 0x1000, MemoryChecker::READWRITE,
                                     "args:EntryPoint:RegistryPath");
    }

    bool pushed = false;
    pushed = changeConsistencyForEntryPoint(state, STRICT, 0);
    incrementEntryPoint(state);

    FUNCMON_REGISTER_RETURN_A(state, fns, WindowsDriverExerciser::DriverEntryPointRet, driverObject, pushed);
}

void WindowsDriverExerciser::DriverEntryPointRet(S2EExecutionState* state, uint32_t pDriverObject, bool pushed)
{
    s2e()->getDebugStream(state) << "Returning from WindowsDriverExerciser entry point "
                << " at " << hexval(state->getPc()) << '\n';

    if (pushed) {
        m_models->pop(state);
    }

    if(m_memoryChecker) {
        m_memoryChecker->revokeMemoryForModule(state, "args:EntryPoint:*");
    }

    const ModuleDescriptor *module = m_detector->getCurrentDescriptor(state);
    assert(module);

    //Check the success status
    klee::ref<klee::Expr> eax = state->readCpuRegister(offsetof(CPUX86State, regs[R_EAX]), klee::Expr::Int32);

    if (!NtSuccess(s2e(), state, eax)) {
        std::stringstream ss;
        ss << "Entry point failed with " << std::hex << eax;
        incrementFailures(state);
        detectLeaks(state, *module);

        s2e()->getExecutor()->terminateStateEarly(*state, ss.str());
        return;
    }

    NtoskrnlHandlers *ntosHandlers = static_cast<NtoskrnlHandlers*>(s2e()->getPlugin("NtoskrnlHandlers"));

    //Register all major functions, only if they belong to the driver itself
    DRIVER_OBJECT32 driverObject;
    bool driverObjectValid = state->readMemoryConcrete(pDriverObject, &driverObject, sizeof(driverObject));

    if (ntosHandlers) {
        if (driverObjectValid) {
            const ModuleDescriptor *desc = module;
    
            for (unsigned i=0; i<IRP_MJ_MAXIMUM_FUNCTION; ++i) {
                if (desc->Contains(driverObject.MajorFunction[i])) {
                    s2e()->getMessagesStream() << "Registering IRP " << s_irpMjArray[i] << " at "
                            << hexval(driverObject.MajorFunction[i]) << " "
                            << desc->Name << "!" << hexval(desc->ToNativeBase(driverObject.MajorFunction[i])) <<
                            '\n';
    
                    ntosHandlers->registerEntryPoint
                            (state, &NtoskrnlHandlers::DriverDispatch, (uint64_t)driverObject.MajorFunction[i], (uint32_t)i);
                }
            }
        }else {
            s2e()->getWarningsStream() << "Could not read DRIVER_OBJECT structure at "
                    << hexval(pDriverObject) << '\n';
        }
    }else {
        s2e()->getWarningsStream() << "NtoskrnlHandlers plugin not loaded. Skipping all IRP annotations." << '\n';
    }

    if (m_memoryChecker) {
        //Windows will unload that section after DriverEntry returns.
        m_memoryChecker->revokeMemoryForModuleSection(state, *module, "INIT");
    }

    if (driverObjectValid && driverObject.DriverUnload) {
        s2e()->getDebugStream() << "Registering " << "DriverUnload" << " at " << hexval(driverObject.DriverUnload) << '\n';
        registerEntryPoint(state, &WindowsDriverExerciser::DriverUnload, driverObject.DriverUnload);
    }

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}


void WindowsDriverExerciser::DriverUnload(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();

    FUNCMON_REGISTER_RETURN(state, fns, WindowsDriverExerciser::DriverUnloadRet)
}

void WindowsDriverExerciser::DriverUnloadRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

}
}
