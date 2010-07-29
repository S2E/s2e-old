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

    m_consistency = cfg->getString(getConfigKey() + ".consistency", "", &ok);
    if (m_consistency != "strict" && m_consistency != "local" && m_consistency != "overapproximate"
        && m_consistency != "overconstrained") {
        s2e()->getWarningsStream() << "Incorrect consistency " << m_hwId << std::endl;
        exit(-1);
    }

    if (m_consistency == "overconstrained") {
        //Make sure to enable the FORCE_CONCRETIZATION hack in S2EExecutor
        m_consistency = "strict";
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

void NdisHandlers::undoCallAndJumpToSymbolic(S2EExecutionState *state)
{
    if (s2e()->getExecutor()->needToJumpToSymbolic(state)) {
        //Undo the call
        assert(state->getTb()->pcOfLastInstr);
        state->setSp(state->getSp() + sizeof(uint32_t));
        state->setPc(state->getTb()->pcOfLastInstr);
        s2e()->getExecutor()->jumpToSymbolicCpp(state);
    }
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
    REGISTER_IMPORT(I, "ndis.sys", NdisMRegisterIoPortRange);

    REGISTER_IMPORT(I, "ntoskrnl.exe", RtlEqualUnicodeString);
    //REGISTER_IMPORT(I, "ntoskrnl.exe", GetSystemUpTime);
    //REGISTER_IMPORT(I, "hal.dll", KeStallExecutionProcessor);

}


bool NdisHandlers::NtSuccess(S2E *s2e, S2EExecutionState *s, klee::ref<klee::Expr> &expr)
{
    bool isTrue;
    klee::ref<klee::Expr> eq = klee::SgeExpr::create(expr, klee::ConstantExpr::create(0, expr.get()->getWidth()));

    if (s2e->getExecutor()->getSolver()->mustBeTrue(klee::Query(s->constraints, eq), isTrue)) {
        return isTrue;
    }
    return false;
}

bool NdisHandlers::bypassFunction(S2EExecutionState *s, unsigned paramCount)
{
    uint32_t retAddr;
    if (!s->readMemoryConcrete(s->getSp(), &retAddr, sizeof(retAddr))) {
        g_s2e->getDebugStream() << "Could not get the return address " << std::endl;
        return false;
    }

    uint32_t newSp = s->getSp() + (paramCount+1)*sizeof(uint32_t);

    s->setSp(newSp);
    s->setPc(retAddr);
    return true;
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
void NdisHandlers::GetSystemUpTime(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getDebugStream(state) << "Bypassing function " << __FUNCTION__ << std::endl;

    klee::ref<klee::Expr> ret = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);

    uint32_t valPtr;
    if (readConcreteParameter(state, 0, &valPtr)) {
        state->writeMemory(valPtr, ret);
        bypassFunction(state, 1);
        throw CpuExitException();
    }
}

void NdisHandlers::KeStallExecutionProcessor(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    s2e()->getDebugStream(state) << "Bypassing function " << __FUNCTION__ << std::endl;

    bypassFunction(state, 1);
    throw CpuExitException();
}


void NdisHandlers::RtlEqualUnicodeString(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (m_consistency == "strict") {
        return;
    }

    undoCallAndJumpToSymbolic(state);

    //XXX: local assumes the stuff comes from the registry
    if (m_consistency == "overapproximate" || m_consistency == "local") {
        klee::ref<klee::Expr> eax = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), eax);
        bypassFunction(state, 3);
        throw CpuExitException();
    }
}


void NdisHandlers::NdisAllocateMemory(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (m_consistency == "strict") {
        return;
    }

    signal->connect(sigc::mem_fun(*this, &NdisHandlers::NdisAllocateMemoryRet));
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

    if (m_consistency == "overapproximate") {
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);
    }else

    //Consistency: LOCAL
    if (m_consistency == "local") {
        /* Fork success and failure */
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        klee::ref<klee::Expr> cond = klee::EqExpr::create(success, klee::ConstantExpr::create(0, klee::Expr::Int32));

        klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*state, cond, false);

        S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

        /* Update each of the states */
        uint32_t retVal = 0;
        ts->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

        retVal = 0xC0000001L;
        fs->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));
    }
}

void NdisHandlers::NdisMRegisterIoPortRange(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (m_consistency == "strict") {
        return;
    }

    signal->connect(sigc::mem_fun(*this, &NdisHandlers::NdisMRegisterIoPortRangeRet));
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

    if (m_consistency == "overapproximate") {
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);
    }else

    //Consistency: LOCAL
    if (m_consistency == "local") {
        /* Fork success and failure */
        klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        klee::ref<klee::Expr> cond = klee::NeExpr::create(success, klee::ConstantExpr::create(0, klee::Expr::Int32));

        klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*state, cond, false);
        S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

        /* Update each of the states */
        //First state succeeded
        uint32_t retVal = 0;
        fs->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

        retVal = 0xc001001E;
        ts->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

#if 0
        //Second state: NDIS_STATUS_RESOURCE_CONFLICT 0xc001001E
        klee::ref<klee::Expr> cond2 = klee::NeExpr::create(success, klee::ConstantExpr::create(0xc001001E, klee::Expr::Int32));
        sp = s2e()->getExecutor()->fork(*ts, cond2, false);
        S2EExecutionState *ts_1 = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs_1 = static_cast<S2EExecutionState *>(sp.second);

        retVal = 0xc001001E;
        fs_1->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

        //Third state: NDIS_STATUS_RESOURCES 0xc000009a
        klee::ref<klee::Expr> cond3 = klee::NeExpr::create(success, klee::ConstantExpr::create(0xc000009a, klee::Expr::Int32));
        sp = s2e()->getExecutor()->fork(*ts_1, cond3, false);
        S2EExecutionState *ts_2 = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs_2 = static_cast<S2EExecutionState *>(sp.second);

        retVal = 0xc000009a;
        fs_2->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));

        //Fourth state: NDIS_STATUS_FAILURE
        retVal = 0xC0000001L;
        ts_2->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &retVal, sizeof(retVal));
#endif
    }

}
#if 0

void NdisHandlers::NdisReadNetworkAddress(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (m_consistency == "strict") {
        return;
    }

    signal->connect(sigc::mem_fun(*this, &NdisHandlers::NdisReadNetworkAddressRet));
}

void NdisHandlers::NdisReadNetworkAddressRet(S2EExecutionState* state)
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


}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::entryPoint(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
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
    klee::ref<klee::Expr> eax = state->readCpuRegister(offsetof(CPUState, regs[R_EAX]), klee::Expr::Int32);

    if (!NtSuccess(s2e(), state, eax)) {
        s2e()->getMessagesStream(state) << "Killing state "  << state->getID() <<
                " because EntryPoint failed with 0x" << std::hex << eax << std::endl;
        s2e()->getExecutor()->terminateStateOnExit(*state);
        return;
    }


    m_manager->succeedState(state);
    if (m_manager->empty()) {
        m_manager->killAllButOneSuccessful();
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

void NdisHandlers::NdisMRegisterMiniport(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    if (!calledFromModule(state)) { return; }
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    if (m_consistency != "strict") {
        signal->connect(sigc::mem_fun(*this, &NdisHandlers::NdisMRegisterMiniportRet));
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

#if 0
    if (m_consistency == "overapproximate") {
        //Replace the return value with a symbolic value
        if ((int)eax>=0) {
            klee::ref<klee::Expr> ret = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
            state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), ret);
        }
    }
#endif

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::CheckForHang(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::NdisMRegisterMiniportRet));
}

void NdisHandlers::CheckForHangRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::InitializeHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::InitializeHandlerRet));

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

    if (m_consistency == "overapproximate") {
        if (pMediumArray) {
            for (unsigned i=0; i<MediumArraySize; i++) {
                std::stringstream ss;
                ss << "MediumArray" << std::dec << "_" << i;
                state->writeMemory(pMediumArray + i * 4, state->createSymbolicValue(klee::Expr::Int32, ss.str()));
            }
            writeParameter(state, 3, state->createSymbolicValue(klee::Expr::Int32, "MediumArraySize"));
        }
    }else

    if (m_consistency == "local") {
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
                " because EntryPoint failed with 0x" << std::hex << eax << std::endl;
        s2e()->getExecutor()->terminateStateOnExit(*state);
        return;
    }


    m_manager->succeedState(state);
    if (m_manager->empty()) {
        m_manager->killAllButOneSuccessful();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::DisableInterruptHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::DisableInterruptHandlerRet));
}

void NdisHandlers::DisableInterruptHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::EnableInterruptHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::EnableInterruptHandlerRet));
}

void NdisHandlers::EnableInterruptHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::HaltHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::HaltHandlerRet));
}

void NdisHandlers::HaltHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    //There is nothing more to execute, kill the state
    s2e()->getExecutor()->terminateStateOnExit(*state);

}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::HandleInterruptHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::HandleInterruptHandlerRet));
}

void NdisHandlers::HandleInterruptHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    m_manager->succeedState(state);
    if (m_manager->empty()) {
        m_manager->killAllButOneSuccessful();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ISRHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::ISRHandlerRet));
    m_devDesc->setInterrupt(false);
}

void NdisHandlers::ISRHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    m_devDesc->setInterrupt(false);
    m_manager->succeedState(state);
    if (m_manager->empty()) {
        m_manager->killAllButOneSuccessful();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::QueryInformationHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::QueryInformationHandlerRet));
}

void NdisHandlers::QueryInformationHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    m_manager->succeedState(state);
    if (m_manager->empty()) {
        m_manager->killAllButOneSuccessful();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ReconfigureHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::ReconfigureHandlerRet));
}

void NdisHandlers::ReconfigureHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ResetHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::ResetHandlerRet));
}

void NdisHandlers::ResetHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    m_manager->succeedState(state);
    if (m_manager->empty()) {
        m_manager->killAllButOneSuccessful();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::SendHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::SendHandlerRet));
}

void NdisHandlers::SendHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    m_devDesc->setInterrupt(true);

    m_manager->succeedState(state);
    if (m_manager->empty()) {
        m_manager->killAllButOneSuccessful();
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::SendPacketsHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::SendPacketsHandlerRet));
}

void NdisHandlers::SendPacketsHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;

    m_manager->succeedState(state);
    if (m_manager->empty()) {
        m_manager->killAllButOneSuccessful();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::SetInformationHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::SetInformationHandlerRet));
}

void NdisHandlers::SetInformationHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::TransferDataHandler(S2EExecutionState* state, FunctionMonitor::ReturnSignal *signal)
{
    s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
    signal->connect(sigc::mem_fun(*this, &NdisHandlers::TransferDataHandlerRet));
}

void NdisHandlers::TransferDataHandlerRet(S2EExecutionState* state)
{
    s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl;
}

} // namespace plugins
} // namespace s2e
