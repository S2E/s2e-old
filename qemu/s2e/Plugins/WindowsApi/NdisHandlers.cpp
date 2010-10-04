extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include "NdisHandlers.h"
#include "Ndis.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>
#include <klee/Solver.h>

#include <iostream>
#include <sstream>

using namespace s2e::windows;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(NdisHandlers, "Basic collection of NDIS API functions.", "NdisHandlers",
                  "FunctionMonitor", "Interceptor", "ModuleExecutionDetector", "StateManager", "SymbolicHardware");


void NdisHandlers::initialize()
{

    ConfigFile *cfg = s2e()->getConfig();

    WindowsApi::initialize();

    m_manager = static_cast<StateManager*>(s2e()->getPlugin("StateManager"));
    m_hw = static_cast<SymbolicHardware*>(s2e()->getPlugin("SymbolicHardware"));

    ConfigFile::string_list mods = cfg->getStringList(getConfigKey() + ".moduleIds");
    if (mods.size() == 0) {
        s2e()->getWarningsStream() << "NDISHANDLERS: No modules to track configured for the NdisHandlers plugin" << std::endl;
        return;
    }

    bool ok;
    m_devDesc = NULL;
    m_hwId = cfg->getString(getConfigKey() + ".hwId", "", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "NDISHANDLERS: You did not configure any symbolic hardware id" << std::endl;
        exit(-1);
    }else {
        m_devDesc = m_hw->findDevice(m_hwId);
        if (!m_devDesc) {
            s2e()->getWarningsStream() << "NDISHANDLERS: The specified hardware device id is invalid " << m_hwId << std::endl;
            exit(-1);
        }
    }

    //Checking the keywords for NdisReadConfiguration whose result will not be replaced with symbolic values
    ConfigFile::string_list ign = cfg->getStringList(getConfigKey() + ".ignoreKeywords");
    m_ignoreKeywords.insert(ign.begin(), ign.end());

    //The multiplicative interval factor slows down the frequency of the registered timers
    //via NdisSetTimer.
    m_timerIntervalFactor = cfg->getInt(getConfigKey() + ".timerIntervalFactor", 1, &ok);

    //What device type do we want to force?
    //This is only for overapproximate consistency
    m_forceAdapterType = cfg->getInt(getConfigKey() + ".forceAdapterType", InterfaceTypeUndefined, &ok);

    foreach2(it, mods.begin(), mods.end()) {
        m_modules.insert(*it);
    }

    m_windowsMonitor->onModuleLoad.connect(
            sigc::mem_fun(*this,
                    &NdisHandlers::onModuleLoad)
            );

    m_windowsMonitor->onModuleUnload.connect(
            sigc::mem_fun(*this,
                    &NdisHandlers::onModuleUnload)
            );

}


bool NdisHandlers::calledFromModule(S2EExecutionState *s)
{
    const ModuleDescriptor *mod = m_detector->getModule(s, s->getTb()->pcOfLastInstr);
    if (!mod) {
        return false;
    }
    const std::string *modId = m_detector->getModuleId(*mod);
    if (!modId) {
        return false;
    }
    return (m_modules.find(*modId) != m_modules.end());
}

void NdisHandlers::onModuleUnload(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    const std::string *s = m_detector->getModuleId(module);
    if (!s || (m_modules.find(*s) == m_modules.end())) {
        //Not the right module we want to intercept
        return;
    }

    //XXX: We might want to monitor multiple modules, so avoid killing
    s2e()->getExecutor()->terminateStateEarly(*state, "Module unloaded");
    return;
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

    //Skip those that were already loaded
    if (m_loadedModules.find(*s) != m_loadedModules.end()) {
        return;
    }

    m_loadedModules.insert(*s);

    //We loaded the module, instrument the entry point
    if (!module.EntryPoint) {
        s2e()->getWarningsStream() << "NdisHandlers: Module has no entry point ";
        module.Print(s2e()->getWarningsStream());
    }

    FunctionMonitor::CallSignal* entryPoint;
    REGISTER_ENTRY_POINT(entryPoint, module.ToRuntime(module.EntryPoint), entryPoint);

    Imports I;
    if (!m_windowsMonitor->getImports(state, module, I)) {
        s2e()->getWarningsStream() << "NdisHandlers: Could not read imports for module ";
        module.Print(s2e()->getWarningsStream());
        return;
    }

    REGISTER_IMPORT(I, "ndis.sys", NdisMRegisterMiniport);

    REGISTER_IMPORT(I, "ndis.sys", NdisAllocateMemory);
    REGISTER_IMPORT(I, "ndis.sys", NdisAllocateMemoryWithTag);
    REGISTER_IMPORT(I, "ndis.sys", NdisMAllocateSharedMemory);
    REGISTER_IMPORT(I, "ndis.sys", NdisMRegisterIoPortRange);
    REGISTER_IMPORT(I, "ndis.sys", NdisMRegisterInterrupt);
    REGISTER_IMPORT(I, "ndis.sys", NdisMMapIoSpace);
    REGISTER_IMPORT(I, "ndis.sys", NdisMQueryAdapterResources);
    REGISTER_IMPORT(I, "ndis.sys", NdisMAllocateMapRegisters);
    REGISTER_IMPORT(I, "ndis.sys", NdisMInitializeTimer);
    REGISTER_IMPORT(I, "ndis.sys", NdisMSetAttributes);
    REGISTER_IMPORT(I, "ndis.sys", NdisMSetAttributesEx);
    REGISTER_IMPORT(I, "ndis.sys", NdisSetTimer);
    REGISTER_IMPORT(I, "ndis.sys", NdisMRegisterAdapterShutdownHandler);
    REGISTER_IMPORT(I, "ndis.sys", NdisReadNetworkAddress);
    REGISTER_IMPORT(I, "ndis.sys", NdisReadConfiguration);
    REGISTER_IMPORT(I, "ndis.sys", NdisReadPciSlotInformation);
    REGISTER_IMPORT(I, "ndis.sys", NdisWritePciSlotInformation);
    REGISTER_IMPORT(I, "ndis.sys", NdisWriteErrorLogEntry);

    REGISTER_IMPORT(I, "ntoskrnl.exe", RtlEqualUnicodeString);
    REGISTER_IMPORT(I, "ntoskrnl.exe", GetSystemUpTime);
    //REGISTER_IMPORT(I, "hal.dll", KeStallExecutionProcessor);

    //m_functionMonitor->getCallSignal(state, -1, -1)->connect(sigc::mem_fun(*this,
    //     &NdisHandlers::TraceCall));
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

//XXX: Put it in a ntoskrnl.exe file
void NdisHandlers::DebugPrint(S2EExecutionState *state, FunctionMonitorState *fns)
{
    //Invoke this function in all contexts
    uint32_t strptr;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &strptr);

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

void NdisHandlers::GetSystemUpTime(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getDebugStream(state) << "Bypassing function " << __FUNCTION__ << std::endl;

    klee::ref<klee::Expr> ret = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);

    uint32_t valPtr;
    if (readConcreteParameter(state, 0, &valPtr)) {
        state->writeMemory(valPtr, ret);
        state->bypassFunction(1);
        throw CpuExitException();
    }
}

void NdisHandlers::KeStallExecutionProcessor(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getDebugStream(state) << "Bypassing function " << __FUNCTION__ << std::endl;

    state->bypassFunction(1);
    throw CpuExitException();
}


void NdisHandlers::RtlEqualUnicodeString(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //XXX: local assumes the stuff comes from the registry
    //XXX: local consistency is broken, because each time gets a new symbolic value,
    //disregarding the string.
    if (getConsistency(__FUNCTION__) == OVERAPPROX || getConsistency(__FUNCTION__) == LOCAL) {
        klee::ref<klee::Expr> eax = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), eax);
        state->bypassFunction(3);
        throw CpuExitException();
    }
}


void NdisHandlers::NdisAllocateMemoryWithTag(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisAllocateMemoryWithTagRet)
}

void NdisHandlers::NdisAllocateMemoryWithTagRet(S2EExecutionState* state)
{
    //Call the normal allocator annotation, since both functions are similar
    NdisAllocateMemoryRet(state);
}

void NdisHandlers::NdisAllocateMemory(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisAllocateMemoryRet)
}

void NdisHandlers::NdisAllocateMemoryRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getExecutor()->jumpToSymbolicCpp(state);

    //Get the return value
    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &eax, sizeof(eax))) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": return status is not concrete" << std::endl;
        return;
    }

    if (eax) {
        //The original function has failed
        return;
    }

    //XXX: this causes problems and false crashes, too slow
    /*if (getConsistency(__FUNCTION__) == OVERAPPROX) {
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);
    }else*/

    //Consistency: LOCAL
    if (getConsistency(__FUNCTION__) == LOCAL || getConsistency(__FUNCTION__) == OVERAPPROX) {
        /* Fork success and failure */
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        klee::ref<klee::Expr> cond = klee::EqExpr::create(success, klee::ConstantExpr::create(0, klee::Expr::Int32));

        klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*state, cond, false);

        S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);
        m_functionMonitor->eraseSp(state == fs ? ts : fs, state->getPc());

        /* Update each of the states */
        uint32_t retVal = 0;
        ts->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

        retVal = 0xC0000001L;
        fs->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

     }
}

void NdisHandlers::NdisMAllocateSharedMemory(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;


    bool ok = true;
    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    ok &= readConcreteParameter(state, 1, &plgState->val3); //Length
    ok &= readConcreteParameter(state, 3, &plgState->val1); //VirtualAddress
    ok &= readConcreteParameter(state, 4, &plgState->val2); //PhysicalAddress

    if (!ok) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": could not read parameters" << std::endl;
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMAllocateSharedMemoryRet)
}

void NdisHandlers::NdisMAllocateSharedMemoryRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getExecutor()->jumpToSymbolicCpp(state);

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    bool ok=true;
    uint32_t va = 0;
    uint64_t pa = 0;

    ok &= state->readMemoryConcrete(plgState->val1, &va, sizeof(va));
    ok &= state->readMemoryConcrete(plgState->val2, &pa, sizeof(pa));
    if (!ok) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": could not read returned addresses" << std::endl;
        s2e()->getWarningsStream() << std::hex << "VirtualAddress=0x" << va << " PhysicalAddress=0x" << pa << std::endl;
        return;
    }

    if (!va) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": original call has failed" << std::endl;
        return;
    }

    //Register symbolic DMA memory.
    //All reads from it will be symbolic.
    m_hw->setSymbolicMmioRange(state, pa, plgState->val3);

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }


    if (getConsistency(__FUNCTION__) == LOCAL || getConsistency(__FUNCTION__) == OVERAPPROX) {
        std::stringstream ss;
        ss << __FUNCTION__ << "_success";
        klee::ref<klee::Expr> succ = state->createSymbolicValue(klee::Expr::Int8, ss.str());
        klee::ref<klee::Expr> cond = klee::EqExpr::create(succ, klee::ConstantExpr::create(1, klee::Expr::Int8));
        klee::ref<klee::Expr> outcome =
                klee::SelectExpr::create(cond, klee::ConstantExpr::create(va, klee::Expr::Int32),
                                             klee::ConstantExpr::create(0, klee::Expr::Int32));
        state->writeMemory(plgState->val1, outcome);

        /* Fork success and failure */
        klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*state, cond, false);

        S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);
        m_functionMonitor->eraseSp(state == fs ? ts : fs, state->getPc());
     }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisMInitializeTimer(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    DECLARE_PLUGINSTATE(NdisHandlersState, state);
    if (!readConcreteParameter(state, 2, &plgState->val1)) {
        s2e()->getDebugStream() << "Could not read function pointer for timer entry point" << std::endl;
        return;
    }

    uint32_t priv;
    if (!readConcreteParameter(state, 3, &priv)) {
        s2e()->getDebugStream() << "Could not read private pointer for timer entry point" << std::endl;
        return;
    }

    s2e()->getDebugStream(state) << "NdisMInitializeTimer pc=0x" << std::hex << plgState->val1 <<
            " priv=0x" << priv << std::endl;

    m_timerEntryPoints.insert(std::make_pair(plgState->val1, priv));

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMInitializeTimerRet)
}

void NdisHandlers::NdisMInitializeTimerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    FunctionMonitor::CallSignal* entryPoint;
    REGISTER_ENTRY_POINT(entryPoint, plgState->val1, NdisTimerEntryPoint);
}

//This annotation will try to run all timer entry points at once to maximize coverage.
//This is only for overapproximate consistency.
void NdisHandlers::NdisTimerEntryPoint(S2EExecutionState* state, FunctionMonitorState *fns)
{
    static TimerEntryPoints exploredRealEntryPoints;
    static TimerEntryPoints exploredEntryPoints;
    TimerEntryPoints scheduled;

    state->undoCallAndJumpToSymbolic();

    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    //state->dumpStack(20, state->getSp());

    uint32_t realPc = state->getPc();
    uint32_t realPriv = 0;
    if (!readConcreteParameter(state, 1, &realPriv)) {
        s2e()->getDebugStream(state) << "Could not read value of opaque pointer" << std::endl;
        return;
    }
    s2e()->getDebugStream(state) << "realPc=0x" << std::hex << realPc << " realpriv=0x" << realPriv << std::endl;

    if (getConsistency(__FUNCTION__) != OVERAPPROX) {
        return;
    }


    //If this this entry point is called for real for the first time,
    //schedule it for execution.
    if (exploredRealEntryPoints.find(std::make_pair(realPc, realPriv)) == exploredRealEntryPoints.end()) {
        s2e()->getDebugStream(state) << "Never called for real, schedule for execution" << std::endl;
        exploredRealEntryPoints.insert(std::make_pair(realPc, realPriv));
        exploredEntryPoints.insert(std::make_pair(realPc, realPriv));
        scheduled.insert(std::make_pair(realPc, realPriv));
    }

    //Compute the set of timer entry points that were never executed.
    //These ones will be scheduled for fake execution.
    TimerEntryPoints scheduleFake;
    std::insert_iterator<TimerEntryPoints > ii(scheduleFake, scheduleFake.begin());
    std::set_difference(m_timerEntryPoints.begin(), m_timerEntryPoints.end(),
                        exploredEntryPoints.begin(), exploredEntryPoints.end(),
                        ii);

    scheduled.insert(scheduleFake.begin(), scheduleFake.end());
    exploredEntryPoints.insert(scheduled.begin(), scheduled.end());

    //If all timers explored, return
    if (scheduled.size() ==0) {
        s2e()->getDebugStream(state) << "No need to redundantly run the timer another time" << std::endl;
        state->bypassFunction(4);
        throw CpuExitException();
    }

    //Must register return handler before forking.
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisTimerEntryPointRet)

    //Fork the number of states corresponding to the number of timers
    std::vector<S2EExecutionState*> states;
    forkStates(state, states, scheduled.size() - 1);
    assert(states.size() == scheduled.size());

    //Fetch the physical address of the first parameter.
    //XXX: This is a hack. S2E does not support writing to virtual memory
    //of inactive states.
    uint32_t param = 1; //We want the second parameter
    uint64_t physAddr = state->getPhysicalAddress(state->getSp() + (param+1) * sizeof(uint32_t));


    //Force the exploration of all registered timer entry points here.
    //This speeds up the exploration
    unsigned stateIdx = 0;
    foreach2(it, scheduled.begin(), scheduled.end()) {
        S2EExecutionState *curState = states[stateIdx++];

        uint32_t pc = (*it).first;
        uint32_t priv = (*it).second;

        s2e()->getDebugStream() << "Found timer 0x" << std::hex << pc << " with private struct 0x" << priv
         << " to explore." << std::endl;

        //Overwrite the original private field with the new one
        klee::ref<klee::Expr> privExpr = klee::ConstantExpr::create(priv, klee::Expr::Int32);
        curState->writeMemory(physAddr, privExpr, S2EExecutionState::PhysicalAddress);

        //Overwrite the program counter with the new handler
        curState->writeCpuState(offsetof(CPUState, eip), pc, sizeof(uint32_t)*8);

        //Mark wheter this state will explore a fake call or a real one.
        DECLARE_PLUGINSTATE(NdisHandlersState, curState);
        plgState->faketimer = !(pc == realPc && priv == realPriv);
    }


}

void NdisHandlers::NdisTimerEntryPointRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    //We must terminate states that ran fake calls, otherwise the system might crash later.
    if (plgState->faketimer) {
        s2e()->getExecutor()->terminateStateEarly(*state, "Terminating state with fake timer call");
    }

    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisSetTimer(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    uint32_t interval;
    if (!readConcreteParameter(state, 1, &interval)) {
        s2e()->getDebugStream() << "Could not read timer interval" << std::endl;
        return;
    }

    //We make the interval longer to avoid overloading symbolic execution.
    //This should give more time for the rest of the system to execute.
    s2e()->getDebugStream() << "Setting interval to " << std::dec << interval*m_timerIntervalFactor
            <<" ms. Was " << interval << " ms." << std::endl;

    klee::ref<klee::Expr> newInterval = klee::ConstantExpr::create(interval*m_timerIntervalFactor, klee::Expr::Int32);
    writeParameter(state, 1, newInterval);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisMRegisterAdapterShutdownHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    DECLARE_PLUGINSTATE(NdisHandlersState, state);
    if (!readConcreteParameter(state, 2, &plgState->val1)) {
        s2e()->getDebugStream() << "Could not read function pointer for timer entry point" << std::endl;
        return;
    }

    plgState->shutdownHandler = plgState->val1;

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMRegisterAdapterShutdownHandlerRet)
}

void NdisHandlers::NdisMRegisterAdapterShutdownHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    FunctionMonitor::CallSignal* entryPoint;
    REGISTER_ENTRY_POINT(entryPoint, plgState->val1, NdisShutdownEntryPoint);
}

void NdisHandlers::NdisShutdownEntryPoint(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisShutdownEntryPointRet)
}

void NdisHandlers::NdisShutdownEntryPointRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisMMapIoSpace(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMMapIoSpaceRet)
}

void NdisHandlers::NdisMMapIoSpaceRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getExecutor()->jumpToSymbolicCpp(state);

    if (getConsistency(__FUNCTION__) == LOCAL) {
        std::vector<uint32_t> values;

        values.push_back(NDIS_STATUS_SUCCESS);
        values.push_back(NDIS_STATUS_RESOURCE_CONFLICT);
        values.push_back(NDIS_STATUS_RESOURCES);
        values.push_back(NDIS_STATUS_FAILURE);
        forkRange(state, __FUNCTION__, values);
    }else  {
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);
    }
}

void NdisHandlers::NdisMAllocateMapRegisters(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMAllocateMapRegistersRet)
}

void NdisHandlers::NdisMAllocateMapRegistersRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getExecutor()->jumpToSymbolicCpp(state);

    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &eax, sizeof(eax))) {
        return;
    }
    if (eax != NDIS_STATUS_SUCCESS) {
        s2e()->getDebugStream(state) <<  __FUNCTION__ << " failed" << std::endl;
        return;
    }

    if (getConsistency(__FUNCTION__) == LOCAL) {
        std::vector<uint32_t> values;

        values.push_back(NDIS_STATUS_SUCCESS);
        values.push_back(NDIS_STATUS_RESOURCES);
        forkRange(state, __FUNCTION__, values);
    }else {
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);
    }
}

void NdisHandlers::NdisMSetAttributesEx(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    klee::ref<klee::Expr> interfaceType = readParameter(state, 4);
    s2e()->getDebugStream(state) << "InterfaceType: " << interfaceType << std::endl;

    if (getConsistency(__FUNCTION__) != OVERAPPROX) {
        return;
    }

    if (((signed)m_forceAdapterType) != InterfaceTypeUndefined) {
        s2e()->getDebugStream(state) << "Forcing NIC type to " << std::dec << m_forceAdapterType << std::endl;
        writeParameter(state, 4, klee::ConstantExpr::create(m_forceAdapterType, klee::Expr::Int32));
    }
}


void NdisHandlers::NdisMSetAttributes(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    klee::ref<klee::Expr> interfaceType = readParameter(state, 3);
    s2e()->getDebugStream(state) << "InterfaceType: " << interfaceType << std::endl;

    if (getConsistency(__FUNCTION__) != OVERAPPROX) {
        return;
    }

    if (((signed)m_forceAdapterType) != InterfaceTypeUndefined) {
        s2e()->getDebugStream(state) << "Forcing NIC type to " << std::dec << m_forceAdapterType << std::endl;
        writeParameter(state, 3, klee::ConstantExpr::create(m_forceAdapterType, klee::Expr::Int32));
    }

}

void NdisHandlers::NdisReadConfiguration(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    //Save parameter data that we will use on return
    //We need to put them in the state-local storage, as parameters can be mangled by the caller
    bool ok = true;
    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    ok &= readConcreteParameter(state, 0, &plgState->pStatus);
    ok &= readConcreteParameter(state, 1, &plgState->pConfigParam);
    ok &= readConcreteParameter(state, 3, &plgState->pConfigString);

    if (!ok) {
        s2e()->getDebugStream() << __FUNCTION__ << " could not read stack parameters (maybe symbolic?) "  << std::endl;
        return;
    }

    uint64_t pc = 0;
    state->getReturnAddress(&pc);
    const ModuleDescriptor *md = m_detector->getModule(state, pc, true);
    if (md) {
        pc = md->ToNativeBase(pc);
    }

    std::string keyword;
    ok = ReadUnicodeString(state, plgState->pConfigString, keyword);
    if (ok) {
        uint32_t paramType;
        ok &= readConcreteParameter(state, 4, &paramType);

        s2e()->getMessagesStream() << "NdisReadConfiguration Module=" << (md ? md->Name : "<unknown>") <<
                " pc=0x" << std::hex << pc <<
                " Keyword=" << keyword <<
            " Type=" << paramType  << std::endl;
    }

    if (m_ignoreKeywords.find(keyword) != m_ignoreKeywords.end()) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisReadConfigurationRet)
}

void NdisHandlers::NdisReadConfigurationRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getExecutor()->jumpToSymbolicCpp(state);

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    if (!plgState->pStatus) {
        s2e()->getDebugStream() << "Status is NULL!" << std::endl;
        return;
    }

    klee::ref<klee::Expr> Status = state->readMemory(plgState->pStatus, klee::Expr::Int32);
    if (!NtSuccess(s2e(), state, Status)) {
        s2e()->getDebugStream() << __FUNCTION__ << " failed with " << Status << std::endl;
        return;
    }


    bool ok = true;
    uint32_t pConfigParam;

    ok &= state->readMemoryConcrete(plgState->pConfigParam, &pConfigParam, sizeof(pConfigParam));
    if (!ok || !pConfigParam) {
        s2e()->getDebugStream() << "Could not read pointer to configuration data" << Status << std::endl;
        return;
    }

    std::string configString;
    ok = ReadUnicodeString(state, plgState->pConfigString, configString);
    if (!ok) {
        s2e()->getDebugStream() << "Could not read keyword string" << std::endl;
    }

    //In all consistency models, inject symbolic value in the parameter that was read
    NDIS_CONFIGURATION_PARAMETER ConfigParam;
    ok = state->readMemoryConcrete(pConfigParam, &ConfigParam, sizeof(ConfigParam));
    if (ok) {
        //For now, we only inject integer values
        if (ConfigParam.ParameterType == NdisParameterInteger || ConfigParam.ParameterType == NdisParameterHexInteger) {
            //Write the symbolic value there.
            uint32_t valueOffset = offsetof(NDIS_CONFIGURATION_PARAMETER, ParameterData);
            std::stringstream ss;
            ss << __FUNCTION__ << "_" << configString << "_value";
            klee::ref<klee::Expr> val = state->createSymbolicValue(klee::Expr::Int32, ss.str());
            state->writeMemory(pConfigParam + valueOffset, val);
        }
    }else {
        s2e()->getDebugStream() << "Could not read configuration data" << Status << std::endl;
        //Continue, this error is not too bad.
    }

    if (getConsistency(__FUNCTION__) == LOCAL) {
        //Fork with either success or failure
        //XXX: Since we cannot write to memory of inactive states, simply create a bunch of select statements
        std::stringstream ss;
        ss << __FUNCTION__ << "_" << configString <<"_success";
        klee::ref<klee::Expr> succ = state->createSymbolicValue(klee::Expr::Bool, ss.str());
        klee::ref<klee::Expr> cond = klee::EqExpr::create(succ, klee::ConstantExpr::create(1, klee::Expr::Bool));
        klee::ref<klee::Expr> outcome =
                klee::SelectExpr::create(cond, klee::ConstantExpr::create(NDIS_STATUS_SUCCESS, klee::Expr::Int32),
                                             klee::ConstantExpr::create(NDIS_STATUS_FAILURE, klee::Expr::Int32));
        state->writeMemory(plgState->pStatus, outcome);

    }else if (getConsistency(__FUNCTION__) == OVERAPPROX) {
        std::stringstream ss;
        ss << __FUNCTION__ << "_success";
        klee::ref<klee::Expr> val = state->createSymbolicValue(klee::Expr::Int32, ss.str());
        state->writeMemory(plgState->pStatus, val);
    }

    plgState->pNetworkAddress = 0;
    plgState->pStatus = 0;
    plgState->pNetworkAddressLength = 0;



}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//BEWARE: This is a VarArg stdecl function...
void NdisHandlers::NdisWriteErrorLogEntry(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    std::ostream &os = s2e()->getDebugStream();

    uint32_t ErrorCode, NumberOfErrorValues;
    bool ok = true;

    ok &= readConcreteParameter(state, 1, &ErrorCode);
    ok &= readConcreteParameter(state, 2, &NumberOfErrorValues);

    if (!ok) {
        os << "Could not read error parameters" << std::endl;
        return;
    }

    os << "ErrorCode=0x" << std::hex << ErrorCode << " - ";

    for (unsigned i=0; i<NumberOfErrorValues; ++i) {
        uint32_t val;
        ok &= readConcreteParameter(state, 3+i, &val);
        if (!ok) {
            os << "Could not read error parameters" << std::endl;
            break;
        }
        os << val << " ";
    }

    os << std::endl;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisReadPciSlotInformation(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    uint32_t offset, buffer, length;
    bool ok = true;

    ok &= readConcreteParameter(state, 2, &offset);
    ok &= readConcreteParameter(state, 3, &buffer);
    ok &= readConcreteParameter(state, 4, &length);
    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters" << std::endl;
    }

    uint64_t retaddr;
    ok = state->getReturnAddress(&retaddr);

    s2e()->getDebugStream(state) << "Buffer=" << std::hex << buffer <<
        " Length=" << std::dec << length << std::endl;

    uint8_t buf[256];
    bool readConcrete = false;
    //Check if we access the base address registers
    if (offset >= 0x10 && offset < 0x28 && (offset - 0x10 + length <= 0x28)) {
        //Get the real concrete values
        readConcrete = m_devDesc->readPciAddressSpace(buf, offset, length);
    }

    //Fill in the buffer with symbolic values
    for (unsigned i=0; i<length; ++i) {
        if (readConcrete) {
            state->writeMemory(buffer + i, &buf[i], klee::Expr::Int8);
        }else {
            std::stringstream ss;
            ss << __FUNCTION__ << "_" << std::hex << retaddr << "_"  << i;
            klee::ref<klee::Expr> symb = state->createSymbolicValue(klee::Expr::Int8, ss.str());
            state->writeMemory(buffer+i, symb);
        }
    }

    //Symbolic return value
    std::stringstream ss;
    ss << __FUNCTION__ << "_" << std::hex << retaddr << "_retval";
    klee::ref<klee::Expr> symb = state->createSymbolicValue(klee::Expr::Int32, ss.str());
    klee::ref<klee::Expr> expr = klee::UleExpr::create(symb, klee::ConstantExpr::create(length, klee::Expr::Int32));
    state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), symb);
    state->addConstraint(expr);

    state->bypassFunction(5);
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisWritePciSlotInformation(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    
    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    uint32_t length;
    bool ok = true;

    ok &= readConcreteParameter(state, 4, &length);
    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters" << std::endl;
    }

    uint64_t retaddr;
    ok = state->getReturnAddress(&retaddr);

    s2e()->getDebugStream(state) << 
        " Length=" << std::dec << length << std::endl;

    //Symbolic return value
    std::stringstream ss;
    ss << __FUNCTION__ << "_" << std::hex << retaddr << "_retval";
    klee::ref<klee::Expr> symb = state->createSymbolicValue(klee::Expr::Int32, ss.str());
    klee::ref<klee::Expr> expr = klee::UleExpr::create(symb, klee::ConstantExpr::create(length, klee::Expr::Int32));
    state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), symb);
    state->addConstraint(expr);

    state->bypassFunction(5);
    throw CpuExitException();
}

void NdisHandlers::NdisMQueryAdapterResources(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    DECLARE_PLUGINSTATE(NdisHandlersState, state);
    
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &plgState->pStatus);

    //XXX: Do the other parameters as well
    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters" << std::endl;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMQueryAdapterResourcesRet)
}

void NdisHandlers::NdisMQueryAdapterResourcesRet(S2EExecutionState* state)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    DECLARE_PLUGINSTATE(NdisHandlersState, state);
    
    if (!plgState->pStatus) {
        s2e()->getDebugStream() << "Status is NULL!" << std::endl;
        return;
    }

    klee::ref<klee::Expr> Status = state->readMemory(plgState->pStatus, klee::Expr::Int32);
    if (!NtSuccess(s2e(), state, Status)) {
        s2e()->getDebugStream() << __FUNCTION__ << " failed with " << Status << std::endl;
        return;
    }

    klee::ref<klee::Expr> SymbStatus = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
    state->writeMemory(plgState->pStatus, SymbStatus);

    return;
}

void NdisHandlers::NdisMRegisterInterrupt(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == OVERAPPROX) {
        //Pretend the interrupt is shared, to force the ISR to be called.
        //Make sure there is indeed a miniportisr registered
        DECLARE_PLUGINSTATE(NdisHandlersState, state);
        if (plgState->hasIsrHandler) {
            s2e()->getDebugStream() << "Pretending that the interrupt is shared." << std::endl;
            //Overwrite the parameter value here
            klee::ref<klee::ConstantExpr> val = klee::ConstantExpr::create(1, klee::Expr::Int32);
            writeParameter(state, 5, val);
        }
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMRegisterInterruptRet)
}

void NdisHandlers::NdisMRegisterInterruptRet(S2EExecutionState* state)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getExecutor()->jumpToSymbolicCpp(state);

    //Get the return value
    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &eax, sizeof(eax))) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": return status is not concrete" << std::endl;
        return;
    }

    if (eax) {
        //The original function has failed
        s2e()->getDebugStream() << __FUNCTION__ << ": original function failed with 0x" << std::hex << eax << std::endl;
        return;
    }

    if (getConsistency(__FUNCTION__) == OVERAPPROX) {
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);
    }else

    //Consistency: LOCAL
    if (getConsistency(__FUNCTION__) == LOCAL) {
        std::vector<uint32_t> values;

        values.push_back(NDIS_STATUS_SUCCESS);
        values.push_back(NDIS_STATUS_RESOURCE_CONFLICT);
        values.push_back(NDIS_STATUS_RESOURCES);
        values.push_back(NDIS_STATUS_FAILURE);
        forkRange(state, __FUNCTION__, values);
    }
}

void NdisHandlers::NdisMRegisterIoPortRange(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMRegisterIoPortRangeRet)
}

void NdisHandlers::NdisMRegisterIoPortRangeRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getExecutor()->jumpToSymbolicCpp(state);

    //Get the return value
    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &eax, sizeof(eax))) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": return status is not concrete" << std::endl;
        return;
    }

    if (eax) {
        //The original function has failed
        return;
    }

    if (getConsistency(__FUNCTION__) == OVERAPPROX) {
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);
    }else

    //Consistency: LOCAL
    if (getConsistency(__FUNCTION__) == LOCAL) {
        std::vector<uint32_t> values;

        values.push_back(NDIS_STATUS_SUCCESS);
        values.push_back(NDIS_STATUS_RESOURCE_CONFLICT);
        values.push_back(NDIS_STATUS_RESOURCES);
        values.push_back(NDIS_STATUS_FAILURE);
        forkRange(state, __FUNCTION__, values);
    }

}

void NdisHandlers::NdisReadNetworkAddress(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    //Save parameter data that we will use on return
    //We need to put them in the state-local storage, as parameters can be mangled by the caller
    bool ok = true;
    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    ok &= readConcreteParameter(state, 0, &plgState->pStatus);
    ok &= readConcreteParameter(state, 1, &plgState->pNetworkAddress);
    ok &= readConcreteParameter(state, 2, &plgState->pNetworkAddressLength);

    if (!ok) {
        s2e()->getDebugStream() << __FUNCTION__ << " could not read stack parameters (maybe symbolic?) "  << std::endl;
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisReadNetworkAddressRet)
}

void NdisHandlers::NdisReadNetworkAddressRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getExecutor()->jumpToSymbolicCpp(state);

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    if (!plgState->pStatus) {
        s2e()->getDebugStream() << "Status is NULL!" << std::endl;
        return;
    }

    klee::ref<klee::Expr> Status = state->readMemory(plgState->pStatus, klee::Expr::Int32);
    if (!NtSuccess(s2e(), state, Status)) {
        s2e()->getDebugStream() << __FUNCTION__ << " failed with " << Status << std::endl;
        return;
    }


    bool ok = true;
    uint32_t Length, NetworkAddress;

    ok &= state->readMemoryConcrete(plgState->pNetworkAddressLength, &Length, sizeof(Length));
    ok &= state->readMemoryConcrete(plgState->pNetworkAddress, &NetworkAddress, sizeof(NetworkAddress));
    if (!ok || !NetworkAddress) {
        s2e()->getDebugStream() << "Could not read network address pointer and/or its length" << Status << std::endl;
        return;
    }

    //In all cases, inject symbolic values in the returned buffer
    for (unsigned i=0; i<Length; ++i) {
        std::stringstream ss;
        ss << __FUNCTION__ << "_" << i;
        klee::ref<klee::Expr> val = state->createSymbolicValue(klee::Expr::Int8, ss.str());
        state->writeMemory(NetworkAddress + i, val);
    }

    if (getConsistency(__FUNCTION__) == LOCAL) {
        //Fork with either success or failure
        //XXX: Since we cannot write to memory of inactive states, simply create a bunch of select statements
        std::stringstream ss;
        ss << __FUNCTION__ << "_success";
        klee::ref<klee::Expr> succ = state->createSymbolicValue(klee::Expr::Bool, ss.str());
        klee::ref<klee::Expr> cond = klee::EqExpr::create(succ, klee::ConstantExpr::create(1, klee::Expr::Bool));
        klee::ref<klee::Expr> outcome =
                klee::SelectExpr::create(cond, klee::ConstantExpr::create(NDIS_STATUS_SUCCESS, klee::Expr::Int32),
                                             klee::ConstantExpr::create(NDIS_STATUS_FAILURE, klee::Expr::Int32));
        state->writeMemory(plgState->pStatus, outcome);

    }else if (getConsistency(__FUNCTION__) == OVERAPPROX) {
        std::stringstream ss;
        ss << __FUNCTION__ << "_success";
        klee::ref<klee::Expr> val = state->createSymbolicValue(klee::Expr::Int32, ss.str());
        state->writeMemory(plgState->pStatus, val);
    }

    plgState->pNetworkAddress = 0;
    plgState->pStatus = 0;
    plgState->pNetworkAddressLength = 0;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::entryPoint(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling NDIS entry point "
                << " at " << hexval(state->getPc()) << std::endl;

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::entryPointRet)
}

void NdisHandlers::entryPointRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from NDIS entry point "
                << " at " << hexval(state->getPc()) << std::endl;

    //Check the success status
    klee::ref<klee::Expr> eax = state->readCpuRegister(offsetof(CPUState, regs[R_EAX]), klee::Expr::Int32);

    if (!NtSuccess(s2e(), state, eax)) {
        s2e()->getMessagesStream(state) << "Killing state "  << state->getID() <<
                " because EntryPoint failed with 0x" << std::hex << eax << std::endl;
        s2e()->getExecutor()->terminateStateEarly(*state, "EntryPoint failed");
        return;
    }

    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

void NdisHandlers::NdisMRegisterMiniport(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) != STRICT) {
        FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMRegisterMiniportRet)
    }

    //Extract the function pointers from the passed data structure
    uint32_t pMiniport;
    if (!state->readMemoryConcrete(state->getSp() + sizeof(pMiniport) * (1+1), &pMiniport, sizeof(pMiniport))) {
        s2e()->getMessagesStream() << "Could not read pMiniport address from the stack" << std::endl;
        return;
    }

    s2e()->getMessagesStream() << "NDIS_MINIPORT_CHARACTERISTICS @0x" << pMiniport << std::endl;

    s2e::windows::NDIS_MINIPORT_CHARACTERISTICS32 Miniport;
    if (!state->readMemoryConcrete(pMiniport, &Miniport, sizeof(Miniport))) {
        s2e()->getMessagesStream() << "Could not read NDIS_MINIPORT_CHARACTERISTICS" << std::endl;
        return;
    }

    //Register each handler
    FunctionMonitor::CallSignal* entryPoint;
    REGISTER_ENTRY_POINT(entryPoint, Miniport.CheckForHangHandler, CheckForHang);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.InitializeHandler, InitializeHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.DisableInterruptHandler, DisableInterruptHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.EnableInterruptHandler, EnableInterruptHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.HaltHandler, HaltHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.HandleInterruptHandler, HandleInterruptHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.ISRHandler, ISRHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.QueryInformationHandler, QueryInformationHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.ReconfigureHandler, ReconfigureHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.ResetHandler, ResetHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.SendHandler, SendHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.SendPacketsHandler, SendPacketsHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.SetInformationHandler, SetInformationHandler);
    REGISTER_ENTRY_POINT(entryPoint, Miniport.TransferDataHandler, TransferDataHandler);

    if (Miniport.ISRHandler) {
        DECLARE_PLUGINSTATE(NdisHandlersState, state);
        plgState->hasIsrHandler = true;
    }
}

void NdisHandlers::NdisMRegisterMiniportRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getExecutor()->jumpToSymbolicCpp(state);

    //Get the return value
    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &eax, sizeof(eax))) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": return status is not concrete" << std::endl;
        return;
    }

    if (getConsistency(__FUNCTION__) == OVERAPPROX) {
        //Replace the return value with a symbolic value
        if ((int)eax>=0) {
            klee::ref<klee::Expr> ret = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
            state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), ret);
        }
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::CheckForHang(S2EExecutionState* state, FunctionMonitorState *fns)
{
    static bool exercised = false;

    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (exercised) {
        uint32_t success = 1;
        state->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &success, sizeof(success));
        state->bypassFunction(1);
        throw CpuExitException();
    }

    exercised = true;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::CheckForHangRet)
}

void NdisHandlers::CheckForHangRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) == OVERAPPROX) {
        //Pretend we did not hang
        //uint32_t success = 0;
        //state->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &success, sizeof(success));
    }

    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::InitializeHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::InitializeHandlerRet)

    /* Make the medium array symbolic */
    uint32_t pMediumArray, MediumArraySize;

    if (!readConcreteParameter(state, 2, &pMediumArray)) {
        s2e()->getDebugStream(state) << "Could not read pMediumArray" << std::endl;
        return;
    }

    if (!readConcreteParameter(state, 3, &MediumArraySize)) {
        s2e()->getDebugStream(state) << "Could not read MediumArraySize" << std::endl;
        return;
    }

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    //if (getConsistency(__FUNCTION__) == LOCAL)
    {
        //Make size properly constrained
        if (pMediumArray) {
            for (unsigned i=0; i<MediumArraySize; i++) {
                std::stringstream ss;
                ss << "MediumArray" << std::dec << "_" << i;
                state->writeMemory(pMediumArray + i * 4, state->createSymbolicValue(klee::Expr::Int32, ss.str()));
            }

            klee::ref<klee::Expr> SymbSize = state->createSymbolicValue(klee::Expr::Int32, "MediumArraySize");

            klee::ref<klee::Expr> Constr = klee::UleExpr::create(SymbSize,
                                                               klee::ConstantExpr::create(MediumArraySize, klee::Expr::Int32));
            state->addConstraint(Constr);
            writeParameter(state, 3, SymbSize);
        }
    }
}

void NdisHandlers::InitializeHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    //Check the success status, kill if failure
    klee::ref<klee::Expr> eax = state->readCpuRegister(offsetof(CPUState, regs[R_EAX]), klee::Expr::Int32);
    klee::Solver *solver = s2e()->getExecutor()->getSolver();
    bool isTrue;
    klee::ref<klee::Expr> eq = klee::EqExpr::create(eax, klee::ConstantExpr::create(0, eax.get()->getWidth()));
    if (!solver->mayBeTrue(klee::Query(state->constraints, eq), isTrue)) {
        s2e()->getMessagesStream(state) << "Killing state "  << state->getID() <<
                " because InitializeHandler failed to determine success" << std::endl;
        s2e()->getExecutor()->terminateStateEarly(*state, "InitializeHandler solver failed");
    }

    if (!isTrue) {
        s2e()->getMessagesStream(state) << "Killing state "  << state->getID() <<
                " because InitializeHandler failed with 0x" << std::hex << eax << std::endl;
        s2e()->getExecutor()->terminateStateEarly(*state, "InitializeHandler failed");
        return;
    }

    //Make sure we succeed by adding a constraint on the eax value
    klee::ref<klee::Expr> constr = klee::SgeExpr::create(eax, klee::ConstantExpr::create(0, eax.get()->getWidth()));
    state->addConstraint(constr);

    s2e()->getDebugStream(state) << "InitializeHandler succeeded with " << eax << std::endl;

    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::DisableInterruptHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::DisableInterruptHandlerRet)
    m_devDesc->setInterrupt(false);
}

void NdisHandlers::DisableInterruptHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::EnableInterruptHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::EnableInterruptHandlerRet)
}

void NdisHandlers::EnableInterruptHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::HaltHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (getConsistency(__FUNCTION__) != OVERAPPROX) {
        FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::HaltHandlerRet)
        return;
    }

    DECLARE_PLUGINSTATE(NdisHandlersState, state);
    if (!plgState->shutdownHandler) {
        FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::HaltHandlerRet);
        return;
    }

    //Fork the states. In one of them run the shutdown handler
    klee::ref<klee::Expr> isFake = state->createSymbolicValue(klee::Expr::Int8, "FakeShutdown");
    klee::ref<klee::Expr> cond = klee::EqExpr::create(isFake, klee::ConstantExpr::create(1, klee::Expr::Int8));

    klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*state, cond, false);

    S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
    S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

    //One of the states will run the shutdown handler
    ts->writeCpuState(offsetof(CPUState, eip), plgState->shutdownHandler, sizeof(uint32_t)*8);

    FUNCMON_REGISTER_RETURN(ts, fns, NdisHandlers::HaltHandlerRet)
    FUNCMON_REGISTER_RETURN(fs, fns, NdisHandlers::HaltHandlerRet)
}

void NdisHandlers::HaltHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    //There is nothing more to execute, kill the state
    s2e()->getExecutor()->terminateStateEarly(*state, "NdisHalt");

}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::HandleInterruptHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::HandleInterruptHandlerRet)
    DECLARE_PLUGINSTATE(NdisHandlersState, state);
   plgState->isrHandlerExecuted = true;
}

void NdisHandlers::HandleInterruptHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ISRHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::ISRHandlerRet)

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    bool ok = true;
    ok &= readConcreteParameter(state, 0, &plgState->isrRecognized);
    ok &= readConcreteParameter(state, 1, &plgState->isrQueue);

    if (!ok) {
        s2e()->getDebugStream(state) << "Error reading isrRecognized and isrQueue"<< std::endl;
    }
    m_devDesc->setInterrupt(false);
}

void NdisHandlers::ISRHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    uint8_t isrRecognized=0, isrQueue=0;
    bool ok = true;
    ok &= state->readMemoryConcrete(plgState->isrRecognized, &isrRecognized, sizeof(isrRecognized));
    ok &= state->readMemoryConcrete(plgState->isrQueue, &isrQueue, sizeof(isrQueue));

    s2e()->getDebugStream(state) << "ISRRecognized=" << (bool)isrRecognized <<
            " isrQueue="<< (bool)isrQueue << std::endl;

    if (!ok) {
        s2e()->getExecutor()->terminateStateEarly(*state, "Could not determine whether the NDIS driver queued the interrupt");
    }else {
        if ((!isrRecognized || !isrQueue) && !plgState->isrHandlerExecuted) {
            s2e()->getExecutor()->terminateStateEarly(*state, "The state will not trigger any interrupt");
        }
    }

    m_devDesc->setInterrupt(false);
    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::QuerySetInformationHandler(S2EExecutionState* state, FunctionMonitorState *fns, bool isQuery)
{
    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    s2e()->getDebugStream() << "Called with OID=0x" << std::hex << plgState->oid << std::endl;

    if (getConsistency(__FUNCTION__) != OVERAPPROX) {
        return;
    }

    std::stringstream ss;
    ss << __FUNCTION__ << "_OID";
    klee::ref<klee::Expr> symbOid = state->createSymbolicValue(klee::Expr::Int32, ss.str());

    klee::ref<klee::Expr> isFakeOid = state->createSymbolicValue(klee::Expr::Int8, "IsFakeOid");
    klee::ref<klee::Expr> cond = klee::EqExpr::create(isFakeOid, klee::ConstantExpr::create(1, klee::Expr::Int8));
/*    klee::ref<klee::Expr> outcome =
            klee::SelectExpr::create(cond, symbOid,
                                         klee::ConstantExpr::create(plgState->oid, klee::Expr::Int32));
*/

    //We fake the stack pointer and prepare symbolic inputs for the buffer
    uint32_t original_sp = state->getSp();
    uint32_t current_sp = original_sp;

    //Create space for the new buffer
    //This will also be present in the original call.
    uint32_t newBufferSize = 64;
    klee::ref<klee::Expr> symbBufferSize = state->createSymbolicValue(klee::Expr::Int32, "QuerySetInfoBufferSize");
    current_sp -= newBufferSize;
    uint32_t newBufferPtr = current_sp;

    //XXX: Do OID-aware injection
    for (unsigned i=0; i<newBufferSize; ++i) {
        std::stringstream ssb;
        ssb << __FUNCTION__ << "_buffer_" << i;
        klee::ref<klee::Expr> symbByte = state->createSymbolicValue(klee::Expr::Int8, ssb.str());
        state->writeMemory(current_sp + i, symbByte);
    }

    //Copy and patch the parameters
    uint32_t origContext, origInfoBuf, origLength, origBytesWritten, origBytesNeeded;
    bool b = true;
    b &= readConcreteParameter(state, 0, &origContext);
    b &= readConcreteParameter(state, 2, &origInfoBuf);
    b &= readConcreteParameter(state, 3, &origLength);
    b &= readConcreteParameter(state, 4, &origBytesWritten);
    b &= readConcreteParameter(state, 5, &origBytesNeeded);

    if (b) {
        current_sp -= sizeof(uint32_t);
        b &= state->writeMemory(current_sp, (uint8_t*)&origBytesNeeded, klee::Expr::Int32);
        current_sp -= sizeof(uint32_t);
        b &= state->writeMemory(current_sp, (uint8_t*)&origBytesWritten, klee::Expr::Int32);

        //Symbolic buffer size
        current_sp -= sizeof(uint32_t);
        b &= state->writeMemory(current_sp, symbBufferSize);


        current_sp -= sizeof(uint32_t);
        b &= state->writeMemory(current_sp, (uint8_t*)&newBufferPtr, klee::Expr::Int32);
        current_sp -= sizeof(uint32_t);
        b &= state->writeMemory(current_sp, symbOid);
        current_sp -= sizeof(uint32_t);
        b &= state->writeMemory(current_sp, (uint8_t*)&origContext, klee::Expr::Int32);

        //Push the new return address (it does not matter normally, so put NULL)
        current_sp -= sizeof(uint32_t);
        uint32_t retaddr = 0x0badcafe;
        b &= state->writeMemory(current_sp,(uint8_t*)&retaddr, klee::Expr::Int32);

    }

    //Fork now, because we do not need to access memory anymore
    klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*state, cond, false);

    S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
    S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

    //Save which state is fake
    DECLARE_PLUGINSTATE_N(NdisHandlersState, ht, ts);
    DECLARE_PLUGINSTATE_N(NdisHandlersState, hf, fs);

    ht->fakeoid = true;
    hf->fakeoid = false;

    //Set the new stack pointer for the fake state
    ts->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_ESP]), &current_sp, sizeof(current_sp));

    //Add a constraint for the buffer size
    klee::ref<klee::Expr> symbBufferConstr = klee::UleExpr::create(symbBufferSize, klee::ConstantExpr::create(newBufferSize, klee::Expr::Int32));
    ts->addConstraint(symbBufferConstr);

    //Register separately the return handler,
    //since the stack pointer is different in the two states
    FUNCMON_REGISTER_RETURN(ts, fns, NdisHandlers::QueryInformationHandlerRet)
    FUNCMON_REGISTER_RETURN(fs, fns, NdisHandlers::QueryInformationHandlerRet)
}

void NdisHandlers::QuerySetInformationHandlerRet(S2EExecutionState* state, bool isQuery)
{
    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    if (plgState->fakeoid) {
        //Stop inconsistent execution immediately
        s2e()->getExecutor()->terminateStateEarly(*state, "Killing state with fake OID");
    }

    s2e()->getDebugStream(state) << "State is not fake, continuing..." << std::endl;

    if (isQuery) {
        //Keep only those states that have a connected cable
        if (plgState->oid == OID_GEN_MEDIA_CONNECT_STATUS) {
            uint32_t status;
            if (state->readMemoryConcrete(plgState->pInformationBuffer, &status, sizeof(status))) {
                s2e()->getDebugStream(state) << "OID_GEN_MEDIA_CONNECT_STATUS is " << status << std::endl;
                if (status == 1) {
                   //Disconnected, kill the state
                   //XXX: For now, we force it to be connected, this is a problem for consistency !!!
                    //It must be connected, otherwise NDIS will not forward any packet to the driver!
                    status = 0;
                    state->writeMemoryConcrete(plgState->pInformationBuffer, &status, sizeof(status));
                  //s2e()->getExecutor()->terminateStateEarly(*state, "Media is disconnected");
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::QueryInformationHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    static bool alreadyExplored = false;
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    plgState->oid = (uint32_t)-1;
    plgState->pInformationBuffer = 0;

    readConcreteParameter(state, 1, &plgState->oid);
    readConcreteParameter(state, 2, &plgState->pInformationBuffer);

    if (alreadyExplored) {
        FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::QueryInformationHandlerRet)
        s2e()->getDebugStream(state) << "Already explored " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
        return;
    }

    state->undoCallAndJumpToSymbolic();

    alreadyExplored = true;
    QuerySetInformationHandler(state, fns, true);

}

void NdisHandlers::QueryInformationHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getExecutor()->jumpToSymbolicCpp(state);

    QuerySetInformationHandlerRet(state, true);

    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::SetInformationHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    static bool alreadyExplored = false;
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    plgState->oid = (uint32_t)-1;
    plgState->pInformationBuffer = 0;

    readConcreteParameter(state, 1, &plgState->oid);
    readConcreteParameter(state, 2, &plgState->pInformationBuffer);

    if (alreadyExplored) {
        FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::SetInformationHandlerRet)
        s2e()->getDebugStream(state) << "Already explored " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
        return;
    }

    state->undoCallAndJumpToSymbolic();

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::SetInformationHandlerRet)

    alreadyExplored = true;
    QuerySetInformationHandler(state, fns, false);
}

void NdisHandlers::SetInformationHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    QuerySetInformationHandlerRet(state, false);

    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ReconfigureHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::ReconfigureHandlerRet)
}

void NdisHandlers::ReconfigureHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ResetHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::ResetHandlerRet)
}

void NdisHandlers::ResetHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::SendHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::SendHandlerRet)
}

void NdisHandlers::SendHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    m_devDesc->setInterrupt(true);

    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::SendPacketsHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::SendPacketsHandlerRet)
}

void NdisHandlers::SendPacketsHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    m_devDesc->setInterrupt(true);

    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::TransferDataHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::TransferDataHandlerRet)
}

void NdisHandlers::TransferDataHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////


NdisHandlersState::NdisHandlersState()
{
    pStatus = 0;
    pNetworkAddress = 0;
    pNetworkAddressLength = 0;
    hasIsrHandler = false;
    fakeoid = false;
    isrRecognized = 0;
    isrQueue = 0;
    isrHandlerExecuted = false;
    faketimer = false;
    shutdownHandler = 0;   
}

NdisHandlersState::~NdisHandlersState()
{

}

NdisHandlersState* NdisHandlersState::clone() const
{
    return new NdisHandlersState(*this);
}

PluginState *NdisHandlersState::factory(Plugin *p, S2EExecutionState *s)
{
    return new NdisHandlersState();
}


} // namespace plugins
} // namespace s2e
