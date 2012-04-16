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

#include <s2e/ConfigFile.h>

#include <s2e/s2e_qemu.h>
#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/S2EExecutor.h>
#include <klee/Solver.h>

#define CURRENT_CLASS Api
#include "Api.h"

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>
#include <s2e/Plugins/ConsistencyModels.h>

#include "NdisHandlers.h"
#include "NtoskrnlHandlers.h"

#include <sstream>

using namespace s2e::windows;


namespace s2e {
namespace plugins {

//Basic init stuff
void WindowsApi::initialize()
{
    m_functionMonitor = static_cast<FunctionMonitor*>(s2e()->getPlugin("FunctionMonitor"));
    m_windowsMonitor = static_cast<WindowsMonitor*>(s2e()->getPlugin("Interceptor"));
    m_detector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));
    m_memoryChecker = static_cast<MemoryChecker*>(s2e()->getPlugin("MemoryChecker"));
    m_manager = static_cast<StateManager*>(s2e()->getPlugin("StateManager"));
    m_bsodInterceptor = static_cast<BlueScreenInterceptor*>(s2e()->getPlugin("BlueScreenInterceptor"));
    m_statsCollector = static_cast<ExecutionStatisticsCollector*>(s2e()->getPlugin("ExecutionStatisticsCollector"));
    m_models = static_cast<ConsistencyModels*>(s2e()->getPlugin("ConsistencyModels"));

    ConfigFile *cfg = s2e()->getConfig();

    m_terminateOnWarnings = cfg->getBool(getConfigKey() + ".terminateOnWarnings");

    parseSpecificConsistency(getConfigKey());
}

void WindowsApi::registerImports(S2EExecutionState *state, const ModuleDescriptor &module)
{
    Imports imports;
    if (!m_windowsMonitor->getImports(state, module, imports)) {
        s2e()->getWarningsStream() << "WindowsApi: Could not read imports for module ";
        module.Print(s2e()->getWarningsStream());
        return;
    }

    //Scan the imports and notify all handler plugins that we need to intercept functions
    foreach2(it, imports.begin(), imports.end()) {
        const std::string &libraryName = (*it).first;
        const ImportedFunctions &functions = (*it).second;

        //XXX: Check that these names are actually in the kernel...
        if (libraryName == "ndis.sys") {
            NdisHandlers *ndisHandlers = static_cast<NdisHandlers*>(s2e()->getPlugin("NdisHandlers"));
            if (!ndisHandlers) {
                s2e()->getWarningsStream() << "NdisHandlers not activated!" << '\n';
            } else {
                ndisHandlers->registerEntryPoints(state, functions);
                ndisHandlers->registerCaller(state, module);
                ndisHandlers->registerImportedVariables(state, module, functions);
            }

        }else if (libraryName == "ntoskrnl.exe") {
            NtoskrnlHandlers *ntoskrnlHandlers = static_cast<NtoskrnlHandlers*>(s2e()->getPlugin("NtoskrnlHandlers"));
            if (!ntoskrnlHandlers) {
                s2e()->getWarningsStream() << "NtoskrnlHandlers not activated!" << '\n';
            } else {
                ntoskrnlHandlers->registerEntryPoints(state, functions);
                ntoskrnlHandlers->registerCaller(state, module);
                ntoskrnlHandlers->registerImportedVariables(state, module, functions);
            }
        }
    }
}

void WindowsApi::parseSpecificConsistency(const std::string &key)
{
    ConfigFile *cfg = s2e()->getConfig();

    //Get the list of key-value pair ids
    bool ok = false;
    ConfigFile::string_list ids = cfg->getListKeys(key + ".functionConsistencies", &ok);

    foreach2(it, ids.begin(), ids.end()) {
        std::string func = *it;

        std::stringstream ss;
        ss << key + ".functionConsistencies." << func;
        ConfigFile::string_list pairs = cfg->getStringList(ss.str());
        if (pairs.size() != 2) {
            s2e()->getDebugStream() << ss.str() << " must have two elements" << '\n';
            exit(-1);
        }

        ExecutionConsistencyModel consistency;
        consistency = ConsistencyModels::fromString(pairs[1]);
        if (consistency == OVERCONSTR) {
            s2e()->getWarningsStream() << "NDISHANDLERS: Cannot handle overconstrained for specific functions\n";
            exit(-1);
        }

        if (consistency == NONE) {
            s2e()->getWarningsStream() << "NDISHANDLERS: Incorrect consistency " << consistency <<
                    " for " << ss.str() << '\n';
            exit(-1);
        }

        s2e()->getDebugStream() << "NDISHANDLERS " << pairs[0] << " will have " << pairs[1] << " consistency" << '\n';
        m_specificConsistency[pairs[0]] = consistency;
    }

}


//////////////////////////////////////////////
bool WindowsApi::NtSuccess(S2E *s2e, S2EExecutionState *state)
{
    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &eax, sizeof(eax))) {
        klee::ref<klee::Expr> val = state->readCpuRegister(offsetof(CPUX86State, regs[R_EAX]), klee::Expr::Int32);
        return NtSuccess(s2e, state, val);
    }
    return ((int32_t)eax >= NDIS_STATUS_SUCCESS);
}

bool WindowsApi::NtSuccess(S2E *s2e, S2EExecutionState *s, klee::ref<klee::Expr> &expr)
{
    bool isTrue;
    klee::ref<klee::Expr> eq = klee::SgeExpr::create(expr, klee::ConstantExpr::create(0, expr.get()->getWidth()));

    if (s2e->getExecutor()->getSolver()->mayBeTrue(klee::Query(s->constraints, eq), isTrue)) {
        return isTrue;
    }
    return false;
}

bool WindowsApi::NtFailure(S2E *s2e, S2EExecutionState *s, klee::ref<klee::Expr> &expr)
{
    bool isTrue;
    klee::ref<klee::Expr> eq = klee::SgeExpr::create(expr, klee::ConstantExpr::create(0, expr.get()->getWidth()));

    if (s2e->getExecutor()->getSolver()->mustBeFalse(klee::Query(s->constraints, eq), isTrue)) {
        return isTrue;
    }
    return false;
}

klee::ref<klee::Expr> WindowsApi::createFailure(S2EExecutionState *state, const std::string &varName)
{
    klee::ref<klee::Expr> symb = state->createSymbolicValue(varName, klee::Expr::Int32);
    klee::ref<klee::Expr> constr = klee::SgtExpr::create(klee::ConstantExpr::create(0, symb.get()->getWidth()), symb);
    state->addConstraint(constr);
    return symb;
}

klee::ref<klee::Expr> WindowsApi::addDisjunctionToConstraints(S2EExecutionState *state, const std::string &varName,
                                                    std::vector<uint32_t> values)
{
    klee::ref<klee::Expr> symb = state->createSymbolicValue(varName, klee::Expr::Int32);
    klee::ref<klee::Expr> constr;

    bool first = true;
    foreach2(it, values.begin(), values.end()) {
        klee::ref<klee::Expr> expr = klee::EqExpr::create(klee::ConstantExpr::create(*it, symb.get()->getWidth()), symb);
        if (first) {
            constr = expr;
            first = false;
        }else {
            constr = klee::OrExpr::create(constr, expr);
        }
    }
    state->addConstraint(constr);
    return symb;
}

//Address is a pointer to a UNICODE_STRING32 structure
bool WindowsApi::ReadUnicodeString(S2EExecutionState *state, uint32_t address, std::string &s)
{
    UNICODE_STRING32 configStringUnicode;
    bool ok;

    ok = state->readMemoryConcrete(address, &configStringUnicode, sizeof(configStringUnicode));
    if (!ok) {
        g_s2e->getDebugStream() << "Could not read UNICODE_STRING32" << '\n';
        return false;
    }

    ok = state->readUnicodeString(configStringUnicode.Buffer, s, configStringUnicode.Length);
    if (!ok) {
        g_s2e->getDebugStream() << "Could not read UNICODE_STRING32"  << '\n';
    }

    return ok;
}

uint32_t WindowsApi::getReturnAddress(S2EExecutionState *s)
{
    uint32_t ra;
    bool b = s->readMemoryConcrete(s->getSp(), &ra, sizeof(ra));
    if (b) {
        return ra;
    } else {
        return 0;
    }
}

bool WindowsApi::readConcreteParameter(S2EExecutionState *s, unsigned param, uint32_t *val)
{
    return s->readMemoryConcrete(s->getSp() + (param+1) * sizeof(uint32_t), val, sizeof(*val));
}

klee::ref<klee::Expr> WindowsApi::readParameter(S2EExecutionState *s, unsigned param)
{
    return s->readMemory(s->getSp() + (param+1) * sizeof(uint32_t), klee::Expr::Int32);
}

bool WindowsApi::writeParameter(S2EExecutionState *s, unsigned param, klee::ref<klee::Expr> val)
{
    return s->writeMemory(s->getSp() + (param+1) * sizeof(uint32_t), val);
}

S2EExecutionState* WindowsApi::forkSuccessFailure(S2EExecutionState *state, bool bypass,
                                                         unsigned argCount,
                                                         const std::string &varName)
{
    klee::ref<klee::Expr> symb = state->createSymbolicValue(varName, klee::Expr::Int32);
    klee::ref<klee::Expr> cond = klee::SgtExpr::create(klee::ConstantExpr::create(STATUS_SUCCESS, klee::Expr::Int32), symb);
    klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*state, cond, false);

    S2EExecutionState *skippedState = static_cast<S2EExecutionState *>(sp.first);
    S2EExecutionState *normalState = static_cast<S2EExecutionState *>(sp.second);

    assert(skippedState == state);

    //symb < STATUS_SUCCESS
    skippedState->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), symb);

    if (bypass) {
        skippedState->bypassFunction(argCount);
    }

    incrementFailures(skippedState);
    return normalState;
}

bool WindowsApi::forkRange(S2EExecutionState *state,
                           const std::string &msg, std::vector<uint32_t> values)
{

    assert(m_functionMonitor);
    bool oldForkStatus = state->isForkingEnabled();
    state->enableForking();

    klee::ref<klee::Expr> success = state->createSymbolicValue(msg, klee::Expr::Int32);

    S2EExecutionState *curState = state;

    for (unsigned i=0; values.size()>0 && i<values.size()-1; ++i) {
        klee::ref<klee::Expr> cond = klee::NeExpr::create(success, klee::ConstantExpr::create(values[i], klee::Expr::Int32));

        klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*curState, cond, false);
        S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

        //XXX: Remove this from here (but check all the callers and return a list of forked states...)
        m_functionMonitor->eraseSp(state == fs ? ts : fs, state->getPc());

        //uint32_t retVal = values[i];
        fs->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), success);
        curState = ts;
        fs->setForking(oldForkStatus);
    }

    uint32_t retVal = values[values.size()-1];
    klee::ref<klee::Expr> cond = klee::EqExpr::create(success, klee::ConstantExpr::create(retVal, klee::Expr::Int32));
    curState->addConstraint(cond);
    curState->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), success);

    curState->setForking(oldForkStatus);
    return true;
}

//Creates count copies of the current state.
//All the states are identical.
//XXX: Should move to S2EExecutor, use forkRange.
klee::ref<klee::Expr> WindowsApi::forkStates(S2EExecutionState *state, std::vector<S2EExecutionState*> &result,
                            int count, const std::string &varName)
{
    bool oldForkStatus = state->isForkingEnabled();
    state->enableForking();

    klee::ref<klee::Expr> symb = state->createSymbolicValue(varName, klee::Expr::Int32);
    std::vector<klee::Expr> conditions;

    S2EExecutionState *curState = state;
    for (int i=0; i<count; ++i) {
        klee::ref<klee::Expr> cond = klee::NeExpr::create(symb, klee::ConstantExpr::create(i, klee::Expr::Int32));

        klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*curState, cond, false);
        S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

        curState = ts;
        result.push_back(fs);
        fs->setForking(oldForkStatus);
    }
    klee::ref<klee::Expr> cond = klee::EqExpr::create(symb, klee::ConstantExpr::create(count, klee::Expr::Int32));
    curState->addConstraint(cond);
    result.push_back(curState);
    curState->setForking(oldForkStatus);
    return symb;
}

//This is meant to be called in a call handler
const std::string WindowsApi::getVariableName(S2EExecutionState *state, const std::string &base)
{
    std::stringstream ss;
    uint64_t pc = state->getTb()->pcOfLastInstr;

    const ModuleDescriptor *desc = m_detector->getModule(state, pc, true);
    if (desc) {
        uint64_t relPc = desc->ToNativeBase(pc);
        ss << base << "_" << desc->Name << "_0x" << std::hex << relPc;
    }else {
        ss << base << "_0x" << std::hex << pc;
    }
    return ss.str();
}


//////////////////////////////////////////////


bool WindowsApi::grantAccessToUnicodeString(S2EExecutionState *state,
                                uint64_t address, const std::string &regionType)
{
    if (!m_memoryChecker) {
        return false;
    }

    m_memoryChecker->grantMemory(state, address, sizeof(UNICODE_STRING32),
                                 MemoryChecker::READWRITE, regionType + ":UnicodeString", address, false);

    UNICODE_STRING32 String;
    if (!state->readMemoryConcrete(address, &String, sizeof(String))) {
        s2e()->getWarningsStream() << "grantAccessToUnicodeString failed" << '\n';
        return false;
    }

    if (String.Buffer && String.MaximumLength > 0) {
        m_memoryChecker->grantMemory(state, String.Buffer, String.MaximumLength * sizeof(uint16_t),
                                     MemoryChecker::READWRITE, regionType+":UnicodeStringBuffer", String.Buffer, false);
    }else {
        s2e()->getWarningsStream() << "grantAccessToUnicodeString: string not initialized properly" << '\n';
        return false;
    }

    return true;
}

bool WindowsApi::revokeAccessToUnicodeString(S2EExecutionState *state,
                                uint64_t address)
{
    if (!m_memoryChecker) {
        return false;
    }

    UNICODE_STRING32 String;
    if (!state->readMemoryConcrete(address, &String, sizeof(String))) {
        s2e()->getWarningsStream() << "revokeAccessToUnicodeString failed" << '\n';
        return false;
    }

    bool res = true;
    res &= m_memoryChecker->revokeMemory(state, "", String.Buffer);
    res &= m_memoryChecker->revokeMemory(state, "", address);

    return res;
}

//Default implementation that simply disconnets all registered functions
void WindowsApi::onModuleUnload(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    m_functionMonitor->disconnect(state, module);
    unregisterEntryPoints(state, module);
    unregisterCaller(state, module);
}


}
}
