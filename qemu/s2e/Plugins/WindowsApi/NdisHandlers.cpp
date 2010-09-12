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

    m_functionMonitor = static_cast<FunctionMonitor*>(s2e()->getPlugin("FunctionMonitor"));
    m_windowsMonitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));
    m_detector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));
    m_manager = static_cast<StateManager*>(s2e()->getPlugin("StateManager"));
    m_hw = static_cast<SymbolicHardware*>(s2e()->getPlugin("SymbolicHardware"));

    ConfigFile::string_list mods = cfg->getStringList(getConfigKey() + ".moduleIds");
    if (mods.size() == 0) {
        s2e()->getWarningsStream() << "No modules to track configured for the NdisHandlers plugin" << std::endl;
        return;
    }

    bool ok;
    m_devDesc = NULL;
    m_hwId = cfg->getString(getConfigKey() + ".hwId", "", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You did not configure any symbolic hardware id" << std::endl;
        exit(-1);
    }else {
        m_devDesc = m_hw->findDevice(m_hwId);
        if (!m_devDesc) {
            s2e()->getWarningsStream() << "The specified hardware device id is invalid " << m_hwId << std::endl;
            exit(-1);
        }
    }

    std::string consistency = cfg->getString(getConfigKey() + ".consistency", "", &ok);
    if (consistency == "strict") {
        m_consistency = STRICT;
    }else if (consistency == "local") {
        m_consistency = LOCAL;
    }else if (consistency == "overapproximate") {
        m_consistency = OVERAPPROX;
    }else if  (consistency == "overconstrained") {
        //This is strict consistency with forced concretizations
        m_consistency = STRICT;
        s2e()->getExecutor()->setForceConcretizations(true);
    }else {
        s2e()->getWarningsStream() << "Incorrect consistency " << consistency << std::endl;
        exit(-1);
    }




    foreach2(it, mods.begin(), mods.end()) {
        m_modules.insert(*it);
    }

    m_windowsMonitor->onModuleLoad.connect(
            sigc::mem_fun(*this,
                    &NdisHandlers::onModuleLoad)
            );


}

void NdisHandlers::registerImport(Imports &I, const std::string &dll, const std::string &name,
                                  FunctionHandler handler, S2EExecutionState *state)
{
    //Register all the relevant imported functions
    Imports::iterator it = I.find(dll);
    if (it == I.end()) {
        s2e()->getWarningsStream() << "NdisHandlers: Could not read imports for " << dll << std::endl;
        return;
    }

    ImportedFunctions &funcs = (*it).second;
    ImportedFunctions::iterator fit = funcs.find(name);
    if (fit == funcs.end()) {
        s2e()->getWarningsStream() << "NdisHandlers: Could not find " << name << " in " << dll << std::endl;
        return;
    }

    s2e()->getMessagesStream() << "Registering import" << name <<  " at 0x" << std::hex << (*fit).second << std::endl;

    FunctionMonitor::CallSignal* cs;
    cs = m_functionMonitor->getCallSignal(state, (*fit).second, 0);
    cs->connect(sigc::mem_fun(*this, handler));
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
    REGISTER_NDIS_ENTRY_POINT(entryPoint, module.ToRuntime(module.EntryPoint), entryPoint);

    Imports I;
    if (!m_windowsMonitor->getImports(state, module, I)) {
        s2e()->getWarningsStream() << "NdisHandlers: Could not read imports for module ";
        module.Print(s2e()->getWarningsStream());
        return;
    }

    REGISTER_IMPORT(I, "ndis.sys", NdisMRegisterMiniport);

    REGISTER_IMPORT(I, "ndis.sys", NdisAllocateMemory);
    REGISTER_IMPORT(I, "ndis.sys", NdisAllocateMemoryWithTag);
    REGISTER_IMPORT(I, "ndis.sys", NdisMRegisterIoPortRange);
    REGISTER_IMPORT(I, "ndis.sys", NdisMRegisterInterrupt);
    REGISTER_IMPORT(I, "ndis.sys", NdisReadNetworkAddress);
    REGISTER_IMPORT(I, "ndis.sys", NdisReadConfiguration);

    REGISTER_IMPORT(I, "ntoskrnl.exe", RtlEqualUnicodeString);
    REGISTER_IMPORT(I, "ntoskrnl.exe", GetSystemUpTime);
    //REGISTER_IMPORT(I, "hal.dll", KeStallExecutionProcessor);

}


bool NdisHandlers::NtSuccess(S2E *s2e, S2EExecutionState *s, klee::ref<klee::Expr> &expr)
{
    bool isTrue;
    klee::ref<klee::Expr> eq = klee::SgeExpr::create(expr, klee::ConstantExpr::create(0, expr.get()->getWidth()));

    if (s2e->getExecutor()->getSolver()->mayBeTrue(klee::Query(s->constraints, eq), isTrue)) {
        return isTrue;
    }
    return false;
}


bool NdisHandlers::readConcreteParameter(S2EExecutionState *s, unsigned param, uint32_t *val)
{
    bool b = s->readMemoryConcrete(s->getSp() + (param+1) * sizeof(uint32_t), val, sizeof(*val));
    if (!b) {
        return false;
    }
    return true;
}

bool NdisHandlers::writeParameter(S2EExecutionState *s, unsigned param, klee::ref<klee::Expr> val)
{
    bool b = s->writeMemory(s->getSp() + (param+1) * sizeof(uint32_t), val);
    if (!b) {
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
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

    if (m_consistency == STRICT) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //XXX: local assumes the stuff comes from the registry
    if (m_consistency == OVERAPPROX || m_consistency == LOCAL) {
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

    if (m_consistency == STRICT) {
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

    if (m_consistency == STRICT) {
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
    /*if (m_consistency == OVERAPPROX) {
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);
    }else*/

    //Consistency: LOCAL
    if (m_consistency == LOCAL || m_consistency == OVERAPPROX) {
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

void NdisHandlers::NdisReadConfiguration(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (m_consistency == STRICT) {
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

    UNICODE_STRING32 configStringUnicode;
    ok &= state->readMemoryConcrete(plgState->pConfigString, &configStringUnicode, sizeof(configStringUnicode));
    if (!ok || !pConfigParam) {
        s2e()->getDebugStream() << "Could not read keyword UNICODE_STRING32" << Status << std::endl;
        return;
    }

    std::string configString;
    ok = state->readUnicodeString(configStringUnicode.Buffer, configString, configStringUnicode.Length);
    if (!ok) {
        s2e()->getDebugStream() << "Could not read keyword string" << Status << std::endl;
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

    if (m_consistency == LOCAL) {
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

    }else if (m_consistency == OVERAPPROX) {
        std::stringstream ss;
        ss << __FUNCTION__ << "_success";
        klee::ref<klee::Expr> val = state->createSymbolicValue(klee::Expr::Int32, ss.str());
        state->writeMemory(plgState->pStatus, val);
    }

    plgState->pNetworkAddress = 0;
    plgState->pStatus = 0;
    plgState->pNetworkAddressLength = 0;



}

void NdisHandlers::NdisMRegisterInterrupt(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (m_consistency == OVERAPPROX) {
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

    if (m_consistency == OVERAPPROX) {
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);
    }else

    //Consistency: LOCAL
    if (m_consistency == LOCAL) {
        /* Fork success and failure */
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        klee::ref<klee::Expr> cond = klee::NeExpr::create(success, klee::ConstantExpr::create(0, klee::Expr::Int32));

        klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*state, cond, false);
        S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);
        m_functionMonitor->eraseSp(state == fs ? ts : fs, state->getPc());

        /* Update each of the states */
        //First state succeeded
        uint32_t retVal = NDIS_STATUS_SUCCESS;
        fs->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

        //Second state: NDIS_STATUS_RESOURCE_CONFLICT 0xc001001E
        klee::ref<klee::Expr> cond2 = klee::NeExpr::create(success, klee::ConstantExpr::create(0xc001001E, klee::Expr::Int32));
        sp = s2e()->getExecutor()->fork(*ts, cond2, false);
        S2EExecutionState *ts_1 = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs_1 = static_cast<S2EExecutionState *>(sp.second);
        m_functionMonitor->eraseSp(state == fs_1 ? ts_1 : fs_1, state->getPc());

        retVal = NDIS_STATUS_RESOURCE_CONFLICT;
        fs_1->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

        //Third state: NDIS_STATUS_RESOURCES 0xc000009a
        klee::ref<klee::Expr> cond3 = klee::NeExpr::create(success, klee::ConstantExpr::create(0xc000009a, klee::Expr::Int32));
        sp = s2e()->getExecutor()->fork(*ts_1, cond3, false);
        S2EExecutionState *ts_2 = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs_2 = static_cast<S2EExecutionState *>(sp.second);
        m_functionMonitor->eraseSp(state == fs_2 ? ts_2 : fs_2, state->getPc());

        retVal = NDIS_STATUS_RESOURCES;
        fs_2->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

        //Fourth state: NDIS_STATUS_FAILURE
        retVal = NDIS_STATUS_FAILURE;
        ts_2->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

    }
}

void NdisHandlers::NdisMRegisterIoPortRange(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (m_consistency == STRICT) {
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

    if (m_consistency == OVERAPPROX) {
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);
    }else

    //Consistency: LOCAL
    if (m_consistency == LOCAL) {
        /* Fork success and failure */
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        klee::ref<klee::Expr> cond = klee::NeExpr::create(success, klee::ConstantExpr::create(0, klee::Expr::Int32));

        klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*state, cond, false);
        S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);
        m_functionMonitor->eraseSp(state == fs ? ts : fs, state->getPc());

        /* Update each of the states */
        //First state succeeded
        uint32_t retVal = NDIS_STATUS_SUCCESS;
        fs->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));


        //Second state: NDIS_STATUS_RESOURCE_CONFLICT 0xc001001E
        klee::ref<klee::Expr> cond2 = klee::NeExpr::create(success, klee::ConstantExpr::create(0xc001001E, klee::Expr::Int32));
        sp = s2e()->getExecutor()->fork(*ts, cond2, false);
        S2EExecutionState *ts_1 = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs_1 = static_cast<S2EExecutionState *>(sp.second);
        m_functionMonitor->eraseSp(state == fs_1 ? ts_1 : fs_1, state->getPc());

        retVal = NDIS_STATUS_RESOURCE_CONFLICT;
        fs_1->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

        //Third state: NDIS_STATUS_RESOURCES 0xc000009a
        klee::ref<klee::Expr> cond3 = klee::NeExpr::create(success, klee::ConstantExpr::create(0xc000009a, klee::Expr::Int32));
        sp = s2e()->getExecutor()->fork(*ts_1, cond3, false);
        S2EExecutionState *ts_2 = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs_2 = static_cast<S2EExecutionState *>(sp.second);
        m_functionMonitor->eraseSp(state == fs_2 ? ts_2 : fs_2, state->getPc());

        retVal = NDIS_STATUS_RESOURCES;
        fs_2->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

        //Fourth state: NDIS_STATUS_FAILURE
        retVal = NDIS_STATUS_FAILURE;
        ts_2->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

    }

}

void NdisHandlers::NdisReadNetworkAddress(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (m_consistency == STRICT) {
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

    if (m_consistency == LOCAL) {
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

    }else if (m_consistency == OVERAPPROX) {
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

    if (m_consistency != STRICT) {
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
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.CheckForHangHandler, CheckForHang);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.InitializeHandler, InitializeHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.DisableInterruptHandler, DisableInterruptHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.EnableInterruptHandler, EnableInterruptHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.HaltHandler, HaltHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.HandleInterruptHandler, HandleInterruptHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.ISRHandler, ISRHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.QueryInformationHandler, QueryInformationHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.ReconfigureHandler, ReconfigureHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.ResetHandler, ResetHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.SendHandler, SendHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.SendPacketsHandler, SendPacketsHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.SetInformationHandler, SetInformationHandler);
    REGISTER_NDIS_ENTRY_POINT(entryPoint, Miniport.TransferDataHandler, TransferDataHandler);

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

    if (m_consistency == OVERAPPROX) {
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
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::CheckForHangRet)
}

void NdisHandlers::CheckForHangRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
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

    if (m_consistency == STRICT) {
        return;
    }

    //if (m_consistency == LOCAL)
    {
        //Make size properly constrained
        if (pMediumArray) {
            for (unsigned i=0; i<MediumArraySize; i++) {
                std::stringstream ss;
                ss << "MediumArray" << std::dec << "_" << i;
                state->writeMemory(pMediumArray + i * 4, state->createSymbolicValue(klee::Expr::Int32, ss.str()));
            }

            klee::ref<klee::Expr> SymbSize = klee::UleExpr::create(state->createSymbolicValue(klee::Expr::Int32, "MediumArraySize"),
                                                               klee::ConstantExpr::create(MediumArraySize, klee::Expr::Int32));
            writeParameter(state, 3, SymbSize);
        }
    }
}

void NdisHandlers::InitializeHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    //Check the success status, kill if failure
    klee::ref<klee::Expr> eax = state->readCpuRegister(offsetof(CPUState, regs[R_EAX]), klee::Expr::Int32);

    if (!NtSuccess(s2e(), state, eax)) {
        s2e()->getMessagesStream(state) << "Killing state "  << state->getID() <<
                " because InitializeHandler failed with 0x" << std::hex << eax << std::endl;
        s2e()->getExecutor()->terminateStateEarly(*state, "InitializeHandler failed");
        return;
    }

    m_manager->succeedState(state);
    m_functionMonitor->eraseSp(state, state->getPc());
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::DisableInterruptHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::DisableInterruptHandlerRet)
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
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::HaltHandlerRet)
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
    m_devDesc->setInterrupt(false);
}

void NdisHandlers::ISRHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

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

    if (m_consistency != OVERAPPROX) {
        return;
    }

    //Fork the current state. One will have the original request, the other the symbolic one.
    std::stringstream ss;
    ss << __FUNCTION__ << "_OID";
    klee::ref<klee::Expr> symbOid = state->createSymbolicValue(klee::Expr::Int32, ss.str());

    klee::ref<klee::Expr> isFakeOid = state->createSymbolicValue(klee::Expr::Bool, "IsFakeOid");
    klee::ref<klee::Expr> cond = klee::EqExpr::create(isFakeOid, klee::ConstantExpr::create(1, klee::Expr::Bool));
    klee::ref<klee::Expr> outcome =
            klee::SelectExpr::create(cond, symbOid,
                                         klee::ConstantExpr::create(plgState->oid, klee::Expr::Int32));

    writeParameter(state, 1, outcome);

    klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*state, cond, false);

    S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
    S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

    //Save which state is fake
    DECLARE_PLUGINSTATE_N(NdisHandlersState, ht, ts);
    DECLARE_PLUGINSTATE_N(NdisHandlersState, hf, fs);

    ht->fakeoid = true;
    hf->fakeoid = false;
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

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::QueryInformationHandlerRet)

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
