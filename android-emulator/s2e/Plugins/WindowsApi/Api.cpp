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
#include <s2e/S2EExecutor.h>
#include <klee/Solver.h>

#define CURRENT_CLASS Api
#include "Api.h"

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>

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

    parseConsistency(getConfigKey());
    parseSpecificConsistency(getConfigKey());
}

FunctionMonitor::CallSignal* WindowsApi::getCallSignalForImport(Imports &I, const std::string &dll, const std::string &name,
                                  S2EExecutionState *state)
{
    //Register all the relevant imported functions
    Imports::iterator it = I.find(dll);
    if (it == I.end()) {
        s2e()->getWarningsStream() << "NdisHandlers: Could not read imports for " << dll << std::endl;
        return NULL;
    }

    ImportedFunctions &funcs = (*it).second;
    ImportedFunctions::iterator fit = funcs.find(name);
    if (fit == funcs.end()) {
        s2e()->getWarningsStream() << "NdisHandlers: Could not find " << name << " in " << dll << std::endl;
        return NULL;
    }

    s2e()->getMessagesStream() << "Registering import" << name <<  " at 0x" << std::hex << (*fit).second << std::endl;


    FunctionMonitor::CallSignal* cs;
    cs = m_functionMonitor->getCallSignal(state, (*fit).second, 0);
    //cs->connect(sigc::mem_fun(*this, handler));
    return cs;
}

void WindowsApi::parseConsistency(const std::string &key)
{
    ConfigFile *cfg = s2e()->getConfig();
    bool ok = false;
    std::string consistency = cfg->getString(key + ".consistency", "", &ok);

    if (consistency == "strict") {
        m_consistency = STRICT;
    }else if (consistency == "local") {
        m_consistency = LOCAL;
    }else if (consistency == "overapproximate") {
        m_consistency = OVERAPPROX;
    }else if  (consistency == "overconstrained") {
        //This is strict consistency with forced concretizations
        //XXX: cannot have multiple plugins with overconstrained
        m_consistency = STRICT;
        s2e()->getExecutor()->setForceConcretizations(true);
    }else {
        s2e()->getWarningsStream() << "Incorrect consistency " << consistency << std::endl;
        exit(-1);
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
            s2e()->getDebugStream() << ss.str() << " must have two elements" << std::endl;
            exit(-1);
        }

        Consistency consistency = STRICT;
        //Check the consistency type
        if (pairs[1] == "strict") {
            consistency = STRICT;
        }else if (pairs[1] == "local") {
            consistency = LOCAL;
        }else if (pairs[1] == "overapproximate") {
            consistency = OVERAPPROX;
        }else if  (pairs[1] == "overconstrained") {
            //This is strict consistency with forced concretizations
            s2e()->getWarningsStream() << "NDISHANDLERS: Cannot handle overconstrained for specific functions " << std::endl;
            exit(-1);
        }else {
            s2e()->getWarningsStream() << "NDISHANDLERS: Incorrect consistency " << consistency <<
                    " for " << ss.str() << std::endl;
            exit(-1);
        }

        s2e()->getDebugStream() << "NDISHANDLERS " << pairs[0] << " will have " << pairs[1] << " consistency" << std::endl;
        m_specificConsistency[pairs[0]] = consistency;
    }

}

WindowsApi::Consistency WindowsApi::getConsistency(const std::string &fcn) const
{
    ConsistencyMap::const_iterator it = m_specificConsistency.find(fcn);
    if (it != m_specificConsistency.end()) {
        return (*it).second;
    }
    return m_consistency;
}


//////////////////////////////////////////////
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

//Address is a pointer to a UNICODE_STRING32 structure
bool WindowsApi::ReadUnicodeString(S2EExecutionState *state, uint32_t address, std::string &s)
{
    UNICODE_STRING32 configStringUnicode;
    bool ok;

    ok = state->readMemoryConcrete(address, &configStringUnicode, sizeof(configStringUnicode));
    if (!ok) {
        g_s2e->getDebugStream() << "Could not read UNICODE_STRING32" << std::endl;
        return false;
    }

    ok = state->readUnicodeString(configStringUnicode.Buffer, s, configStringUnicode.Length);
    if (!ok) {
        g_s2e->getDebugStream() << "Could not read UNICODE_STRING32"  << std::endl;
    }

    return ok;
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

bool WindowsApi::forkRange(S2EExecutionState *state, const std::string &msg, std::vector<uint32_t> values)
{

    assert(m_functionMonitor);
    bool oldForkStatus = state->isForkingEnabled();
    state->enableForking();

    klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, msg);

    S2EExecutionState *curState = state;

    for (unsigned i=0; values.size()>0 && i<values.size()-1; ++i) {
        klee::ref<klee::Expr> cond = klee::NeExpr::create(success, klee::ConstantExpr::create(values[i], klee::Expr::Int32));

        klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*curState, cond, false);
        S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

        //XXX: Remove this from here.
        m_functionMonitor->eraseSp(state == fs ? ts : fs, state->getPc());

        //uint32_t retVal = values[i];
        fs->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);
        curState = ts;
        fs->setForking(oldForkStatus);
    }

    uint32_t retVal = values[values.size()-1];
    klee::ref<klee::Expr> cond = klee::EqExpr::create(success, klee::ConstantExpr::create(retVal, klee::Expr::Int32));
    curState->addConstraint(cond);
    curState->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), success);

    curState->setForking(oldForkStatus);
    return true;
}

//Creates count copies of the current state.
//All the states are identical.
//XXX: Should move to S2EExecutor, use forkRange.
void WindowsApi::forkStates(S2EExecutionState *state, std::vector<S2EExecutionState*> &result, int count)
{
    bool oldForkStatus = state->isForkingEnabled();
    state->enableForking();

    klee::ref<klee::Expr> success = state->createSymbolicValue(klee::Expr::Int32, "forkStates");
    std::vector<klee::Expr> conditions;

    S2EExecutionState *curState = state;
    for (int i=0; i<count; ++i) {
        klee::ref<klee::Expr> cond = klee::NeExpr::create(success, klee::ConstantExpr::create(i, klee::Expr::Int32));

        klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*curState, cond, false);
        S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
        S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

        curState = ts;
        result.push_back(fs);
        fs->setForking(oldForkStatus);
    }
    klee::ref<klee::Expr> cond = klee::EqExpr::create(success, klee::ConstantExpr::create(count, klee::Expr::Int32));
    curState->addConstraint(cond);
    result.push_back(curState);
    curState->setForking(oldForkStatus);

}


//////////////////////////////////////////////
void WindowsApi::TraceCall(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!m_detector) {
        return;
    }


    std::ostream &os = s2e()->getDebugStream(state);

    const ModuleDescriptor *md = m_detector->getModule(state, state->getPc(), false);

    os << __FUNCTION__ << " ESP=0x" << std::hex << state->readCpuRegister(offsetof(CPUState, regs[R_ESP]), klee::Expr::Int32)
            << " EIP=0x" << state->getPc();

    if (md) {
        os << " " << md->Name << " 0x" << md->ToNativeBase(state->getPc()) << " +" <<
                md->ToRelative(state->getPc());
    }

    os << std::endl;

    FUNCMON_REGISTER_RETURN(state, fns, WindowsApi::TraceRet)
}

void WindowsApi::TraceRet(S2EExecutionState* state)
{
    if (!m_detector) {
        return;
    }

    std::ostream &os = s2e()->getDebugStream(state);

    const ModuleDescriptor *md = m_detector->getModule(state, state->getPc(), false);

    os << __FUNCTION__ << " ESP=0x" << std::hex << state->readCpuRegister(offsetof(CPUState, regs[R_ESP]), klee::Expr::Int32)
            << " EIP=0x" << state->getPc();

    if (md) {
        os << " " << md->Name << " 0x" << md->ToNativeBase(state->getPc()) << " +" <<
                md->ToRelative(state->getPc());
    }

    os << std::endl;
}

}
}
