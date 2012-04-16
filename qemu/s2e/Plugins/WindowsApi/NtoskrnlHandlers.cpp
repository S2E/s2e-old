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
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <s2e/Plugins/FunctionMonitor.h>

#define CURRENT_CLASS NtoskrnlHandlers

#include "NtoskrnlHandlers.h"
#include "Ntddk.h"

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>
#include <klee/Solver.h>

#include <iostream>
#include <sstream>

using namespace s2e::windows;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(NtoskrnlHandlers, "Basic collection of NT Kernel API functions.", "NtoskrnlHandlers",
                  "FunctionMonitor", "Interceptor", "ConsistencyModels");

const NtoskrnlHandlers::AnnotationsArray NtoskrnlHandlers::s_handlers[] = {

    DECLARE_EP_STRUC(NtoskrnlHandlers, DebugPrint),
    DECLARE_EP_STRUC(NtoskrnlHandlers, IoCreateSymbolicLink),

    DECLARE_EP_STRUC(NtoskrnlHandlers, IoCreateDevice),
    DECLARE_EP_STRUC(NtoskrnlHandlers, IoDeleteDevice),

    DECLARE_EP_STRUC(NtoskrnlHandlers, IoIsWdmVersionAvailable),
    DECLARE_EP_STRUC(NtoskrnlHandlers, IoFreeMdl),

    DECLARE_EP_STRUC(NtoskrnlHandlers, RtlEqualUnicodeString),
    DECLARE_EP_STRUC(NtoskrnlHandlers, RtlAddAccessAllowedAce),
    DECLARE_EP_STRUC(NtoskrnlHandlers, RtlCreateSecurityDescriptor),
    DECLARE_EP_STRUC(NtoskrnlHandlers, RtlSetDaclSecurityDescriptor),
    DECLARE_EP_STRUC(NtoskrnlHandlers, RtlAbsoluteToSelfRelativeSD),

    DECLARE_EP_STRUC(NtoskrnlHandlers, GetSystemUpTime),
    DECLARE_EP_STRUC(NtoskrnlHandlers, KeStallExecutionProcessor),

    DECLARE_EP_STRUC(NtoskrnlHandlers, ExAllocatePoolWithTag),
    DECLARE_EP_STRUC(NtoskrnlHandlers, ExFreePool),
    DECLARE_EP_STRUC(NtoskrnlHandlers, ExFreePoolWithTag),

    DECLARE_EP_STRUC(NtoskrnlHandlers, IofCompleteRequest),

    DECLARE_EP_STRUC(NtoskrnlHandlers, DebugPrint),

    DECLARE_EP_STRUC(NtoskrnlHandlers, MmGetSystemRoutineAddress)
};

const char * NtoskrnlHandlers::s_ignoredFunctionsList[] = {
    //XXX: Should revoke read/write grants for these APIs
    "RtlFreeUnicodeString",
    "RtlInitUnicodeString",

    //Other functions
    "IoReleaseCancelSpinLock",
    "IofCompleteRequest",
    "KeBugCheckEx",
    "KeEnterCriticalRegion", "KeLeaveCriticalRegion",
    "RtlLengthSecurityDescriptor",
    "RtlLengthSid",

    "memcpy", "memset", "wcschr", "_alldiv", "_alldvrm", "_aulldiv", "_aulldvrm",
    "_snwprintf", "_wcsnicmp",

    NULL
};

const SymbolDescriptor NtoskrnlHandlers::s_exportedVariablesList[] = {
    {"IoDeviceObjectType", sizeof(uint32_t)},
    {"KeTickCount", sizeof(uint32_t)},
    {"SeExports", sizeof(s2e::windows::SE_EXPORTS)},
    {"", 0}
};


//XXX: These should be implemented:
//IoDeleteSymbolicLink, KeQueryInterruptTime, KeQuerySystemTime, MmGetSystemRoutineAddress,
//MmMapLockedPagesSpecifyCache, ObOpenObjectByPointer, RtlGetDaclSecurityDescriptor,
//RtlGetGroupSecurityDescriptor, RtlGetOwnerSecurityDescriptor, RtlGetSaclSecurityDescriptor,
//SeCaptureSecurityDescriptor

//Registry:
//ZwClose, ZwCreateKey, ZwOpenKey, ZwQueryValueKey, ZwSetSecurityObject, ZwSetValueKey

const NtoskrnlHandlers::AnnotationsMap NtoskrnlHandlers::s_handlersMap =
        NtoskrnlHandlers::initializeHandlerMap();

const NtoskrnlHandlers::StringSet NtoskrnlHandlers::s_ignoredFunctions =
        NtoskrnlHandlers::initializeIgnoredFunctionSet();

const SymbolDescriptors NtoskrnlHandlers::s_exportedVariables =
        NtoskrnlHandlers::initializeExportedVariables();

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
    assert(false);

    m_functionMonitor->disconnect(state, module);
}

std::string NtoskrnlHandlers::readUnicodeString(S2EExecutionState *state, uint32_t pUnicodeString)
{
    UNICODE_STRING32 UnicodeString;

    if (!state->readMemoryConcrete(pUnicodeString, &UnicodeString, sizeof(UnicodeString))) {
        return "";
    }

    std::string ret;
    state->readUnicodeString(UnicodeString.Buffer, ret, UnicodeString.Length);
    return ret;
}

void NtoskrnlHandlers::DebugPrint(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }

    //Invoke this function in all contexts
     uint32_t strptr;
     bool ok = true;
     ok &= readConcreteParameter(state, 1, &strptr);

     if (!ok) {
         s2e()->getDebugStream() << "Could not read string in DebugPrint" << '\n';
         return;
     }

     std::string message;
     ok = state->readString(strptr, message, 255);
     if (!ok) {
         s2e()->getDebugStream() << "Could not read string in DebugPrint at address " << hexval(strptr) <<  '\n';
         return;
     }

     s2e()->getMessagesStream(state) << "DebugPrint: " << message << '\n';
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::IoCreateSymbolicLink(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency < LOCAL) {
        return;
    }

    uint32_t pSymbolicLinkName, pDeviceName;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &pSymbolicLinkName);
    ok &= readConcreteParameter(state, 0, &pDeviceName);
    if (!ok) {
        HANDLER_TRACE_PARAM_FAILED("SymbolicLinkName or DeviceName" );
    }else {
        std::string strSymbolicLinkName = readUnicodeString(state, pSymbolicLinkName);
        std::string strDeviceName = readUnicodeString(state, pDeviceName);

        s2e()->getMessagesStream() << "IoCreateSymbolicName SymbolicLinkName: " << strSymbolicLinkName
                << " DeviceName: " << strDeviceName << '\n';
    }

    state->undoCallAndJumpToSymbolic();

    S2EExecutionState *normalState = forkSuccessFailure(state, true, 2, getVariableName(state, __FUNCTION__));
    FUNCMON_REGISTER_RETURN(normalState, fns, NtoskrnlHandlers::IoCreateSymbolicLinkRet);
}

void NtoskrnlHandlers::IoCreateSymbolicLinkRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();

    if (!NtSuccess(g_s2e, state)) {
        HANDLER_TRACE_FCNFAILED();
        incrementFailures(state);
        return;
    } else {
        incrementSuccesses(state);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::IoCreateDevice(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    uint32_t pDeviceObject;
    if (!readConcreteParameter(state, 6, &pDeviceObject)) {
        HANDLER_TRACE_PARAM_FAILED("pDeviceObject");
        return;
    }

    if (consistency < LOCAL) {
        FUNCMON_REGISTER_RETURN_A(state, fns, NtoskrnlHandlers::IoCreateDeviceRet, pDeviceObject);
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Skip the call in the current state
    state->bypassFunction(7);

    klee::ref<klee::Expr> symb = createFailure(state, getVariableName(state, __FUNCTION__) + "_result");
    state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), symb);
    incrementFailures(state);

    //Register the return handler
    S2EExecutionState *otherState = states[0] == state ? states[1] : states[0];
    FUNCMON_REGISTER_RETURN_A(otherState, m_functionMonitor, NtoskrnlHandlers::IoCreateDeviceRet, pDeviceObject);
}

void NtoskrnlHandlers::IoCreateDeviceRet(S2EExecutionState* state, uint32_t pDeviceObject)
{
    HANDLER_TRACE_RETURN();

    if (!NtSuccess(g_s2e, state)) {
        HANDLER_TRACE_FCNFAILED();
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);

    if (m_memoryChecker) {
        s2e()->getDebugStream() << "IoCreateDeviceRet pDeviceObject=" << hexval(pDeviceObject) << '\n';

        uint32_t DeviceObject;

        if (!state->readMemoryConcrete(pDeviceObject, &DeviceObject, sizeof(DeviceObject))) {
            return;
        }

        m_memoryChecker->grantMemory(state, DeviceObject, sizeof(DEVICE_OBJECT32),
                                     MemoryChecker::READWRITE, "IoCreateDevice", DeviceObject);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::IoDeleteDevice(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    uint32_t pDeviceObject;
    if (!readConcreteParameter(state, 0, &pDeviceObject)) {
        HANDLER_TRACE_PARAM_FAILED("pDeviceObject");
        return;
    }

    if (m_memoryChecker) {
        if (!m_memoryChecker->revokeMemory(state, "IoCreateDevice", pDeviceObject)) {
            s2e()->getExecutor()->terminateStateEarly(*state, "Could not revoke memory for device object");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::IoIsWdmVersionAvailable(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency < LOCAL) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NtoskrnlHandlers::IoIsWdmVersionAvailableRet);
}

void NtoskrnlHandlers::IoIsWdmVersionAvailableRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();

    state->jumpToSymbolicCpp();

    uint32_t isAvailable;
    if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &isAvailable, sizeof(isAvailable))) {
        return;
    }
    if (!isAvailable) {
        HANDLER_TRACE_FCNFAILED();
    }

    //Fork a state with success and failure
    std::vector<uint32_t> values;
    values.push_back(1);
    values.push_back(0);
    forkRange(state, __FUNCTION__, values);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::IoFreeMdl(S2EExecutionState *state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    uint32_t Buffer;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &Buffer);

    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters" << '\n';
        return;
    }

    if(m_memoryChecker) {
        m_memoryChecker->revokeMemory(state, Buffer, uint64_t(-1));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::GetSystemUpTime(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();
    state->undoCallAndJumpToSymbolic();

    s2e()->getDebugStream(state) << "Bypassing function " << __FUNCTION__ << '\n';

    klee::ref<klee::Expr> ret = state->createSymbolicValue(getVariableName(state, __FUNCTION__), klee::Expr::Int32);

    uint32_t valPtr;
    if (readConcreteParameter(state, 0, &valPtr)) {
        state->writeMemory(valPtr, ret);
        state->bypassFunction(1);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::KeStallExecutionProcessor(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();
    s2e()->getDebugStream(state) << "Bypassing function " << __FUNCTION__ << '\n';

    state->bypassFunction(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::RtlEqualUnicodeString(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency == STRICT) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //XXX: local assumes the stuff comes from the registry
    //XXX: local consistency is broken, because each time gets a new symbolic value,
    //disregarding the string.
    if (consistency == OVERAPPROX || consistency == LOCAL) {
        klee::ref<klee::Expr> eax = state->createSymbolicValue(__FUNCTION__, klee::Expr::Int32);
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), eax);
        state->bypassFunction(3);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::RtlAddAccessAllowedAce(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency < LOCAL) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Skip the call in the current state
    state->bypassFunction(4);
    incrementFailures(state);

    state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]),
                            createFailure(state, getVariableName(state, __FUNCTION__) + "_result"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::MmGetSystemRoutineAddress(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency < LOCAL) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Skip the call in the current state
    state->bypassFunction(1);
    incrementFailures(state);

    klee::ref<klee::Expr> symb;
    std::vector<uint32_t> vec;
    vec.push_back(0);
    symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);

    state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), symb);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::RtlCreateSecurityDescriptor(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency < LOCAL) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    if (consistency == OVERAPPROX) {
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]),
                                createFailure(state, getVariableName(state, __FUNCTION__) + "_result"));
    }else {
        //Put bogus stuff in the structure
        uint32_t pSecurityDescriptor = 0;
        if (readConcreteParameter(state, 0, &pSecurityDescriptor)) {
            SECURITY_DESCRIPTOR32 secDesc;
            memset(&secDesc, -1, sizeof(SECURITY_DESCRIPTOR32));
            secDesc.Control = 0;
            state->writeMemoryConcrete(pSecurityDescriptor, &secDesc, sizeof(secDesc));
        }

        std::vector<uint32_t> vec;
        vec.push_back(STATUS_UNKNOWN_REVISION);
        klee::ref<klee::Expr> symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), symb);

        //Skip the call in the current state
        state->bypassFunction(2);
        incrementFailures(state);
    }

}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::RtlSetDaclSecurityDescriptor(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency < LOCAL) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    if (consistency == OVERAPPROX) {
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]),
                                createFailure(state, getVariableName(state, __FUNCTION__) + "_result"));
    }else {
        std::vector<uint32_t> vec;
        vec.push_back(STATUS_UNKNOWN_REVISION);
        vec.push_back(STATUS_INVALID_SECURITY_DESCR);
        klee::ref<klee::Expr> symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), symb);
    }

    //Skip the call in the current state
    state->bypassFunction(4);
    incrementFailures(state);

    //Register the return handler
    S2EExecutionState *otherState = states[0] == state ? states[1] : states[0];
    FUNCMON_REGISTER_RETURN(otherState, m_functionMonitor, NtoskrnlHandlers::RtlSetDaclSecurityDescriptorRet);
}

void NtoskrnlHandlers::RtlSetDaclSecurityDescriptorRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    uint32_t res;
    if (state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &res, sizeof(res))) {
        if ((int)res < 0) {
            HANDLER_TRACE_FCNFAILED_VAL(res);
            incrementFailures(state);
        } else {
            incrementSuccesses(state);
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::RtlAbsoluteToSelfRelativeSD(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency < LOCAL) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    if (consistency == OVERAPPROX) {
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]),
                                createFailure(state, getVariableName(state, __FUNCTION__) + "_result"));
    }else {
        std::vector<uint32_t> vec;
        vec.push_back(STATUS_BAD_DESCRIPTOR_FORMAT);
        vec.push_back(STATUS_BUFFER_TOO_SMALL);
        klee::ref<klee::Expr> symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), symb);
    }

    //Skip the call in the current state
    state->bypassFunction(3);
    incrementFailures(state);

    //Register the return handler
    S2EExecutionState *otherState = states[0] == state ? states[1] : states[0];
    FUNCMON_REGISTER_RETURN(otherState, m_functionMonitor, NtoskrnlHandlers::RtlAbsoluteToSelfRelativeSDRet);
}

void NtoskrnlHandlers::RtlAbsoluteToSelfRelativeSDRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    uint32_t res;
    if (state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &res, sizeof(res))) {
        if ((int)res < 0) {
            HANDLER_TRACE_FCNFAILED_VAL(res);
            incrementFailures(state);
        } else {
            incrementSuccesses(state);
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::ExAllocatePoolWithTag(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    state->undoCallAndJumpToSymbolic();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    bool ok = true;
    uint32_t poolType, size;
    ok &= readConcreteParameter(state, 0, &poolType);
    ok &= readConcreteParameter(state, 1, &size);
    if(!ok) {
        s2e()->getDebugStream(state) << "Can not read pool type and length of memory allocation" << '\n';
        return;
    }

    if (consistency < LOCAL) {
        //We'll have to grant access to the memory array
        FUNCMON_REGISTER_RETURN_A(state, fns, NtoskrnlHandlers::ExAllocatePoolWithTagRet, poolType, size);
        return;
    }

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Skip the call in the current state
    state->bypassFunction(4);
    uint32_t failValue = 0;
    state->writeCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &failValue, sizeof(failValue));
    incrementFailures(state);

    //Register the return handler
    S2EExecutionState *otherState = states[0] == state ? states[1] : states[0];
    FUNCMON_REGISTER_RETURN_A(otherState, m_functionMonitor, NtoskrnlHandlers::ExAllocatePoolWithTagRet, poolType, size);
}

void NtoskrnlHandlers::ExAllocatePoolWithTagRet(S2EExecutionState* state, uint32_t poolType, uint32_t size)
{
    HANDLER_TRACE_RETURN();

    uint32_t address;
    if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &address, sizeof(address))) {
        return;
    }
    if (!address) {
        HANDLER_TRACE_FCNFAILED();
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);

    if (m_memoryChecker) {
        m_memoryChecker->grantMemory(state, address, size, MemoryChecker::READWRITE,
                                     "AllocatePool", address);
    }
}

void NtoskrnlHandlers::ExFreePool(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    if (m_memoryChecker) {
        uint32_t pointer;
        if (readConcreteParameter(state, 0, &pointer)) {
            if (!m_memoryChecker->revokeMemory(state, "AllocatePool", pointer)) {
                s2e()->getExecutor()->terminateStateEarly(*state, "Could not revoke memory");
            }
        }
    }
}

void NtoskrnlHandlers::ExFreePoolWithTag(S2EExecutionState* state, FunctionMonitorState *fns)
{
    ExFreePool(state, fns);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::DispatchWrite(S2EExecutionState* state,
                                     uint64_t pIrp,
                                     const IRP &irp,
                                     const IO_STACK_LOCATION &stackLocation)
{
   #if 0
    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    //Allocate permissions


    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_fakewrite");

    S2EExecutionState *realState = states[0] == state ? states[1] : states[0];
    S2EExecutionState *fakeState = state;
#endif

}


////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::DispatchIoctl(S2EExecutionState* state,
                                     uint64_t pIrp,
                                     const IRP &irp,
                                     const IO_STACK_LOCATION &stackLocation)
{
    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    uint32_t ioctl = stackLocation.Parameters.DeviceIoControl.IoControlCode;
    uint32_t ioctlOffset = offsetof(IO_STACK_LOCATION, Parameters.DeviceIoControl.IoControlCode);

    s2e()->getMessagesStream() << "IOCTL code " << hexval(ioctl) << '\n';

    uint64_t inputLength = stackLocation.Parameters.DeviceIoControl.InputBufferLength;
    uint64_t outputLength = stackLocation.Parameters.DeviceIoControl.OutputBufferLength;
    uint64_t inputBuffer = 0;

    std::stringstream regionType;
    regionType << "DispatchDeviceControl:IRP_MJ_DEVICE_IOCTL:" << hexval(pIrp) << ":";

    //Grant privileges to the data buffers
    switch (ioctl & 0x3) {
        case METHOD_BUFFERED: {
            regionType << "METHOD_BUFFERED:Buffer";
            uint64_t buffer = irp.AssociatedIrp.SystemBuffer;

            uint64_t length = inputLength < outputLength ? outputLength : inputLength;
            if (m_memoryChecker && buffer && length > 0) {
                m_memoryChecker->grantMemory(state, buffer,
                                             length, MemoryChecker::READWRITE,
                                             regionType.str());
            }

            inputBuffer = buffer;
        }
        break;

        case METHOD_OUT_DIRECT:
        case METHOD_IN_DIRECT: {
            regionType << "METHOD_INOUT_DIRECT:";
            //Reading of input data can be done directly
            inputBuffer = irp.AssociatedIrp.SystemBuffer;
            if (m_memoryChecker && inputBuffer && inputLength > 0) {
                m_memoryChecker->grantMemory(state, inputBuffer,
                                             inputLength, MemoryChecker::READWRITE,
                                             regionType.str() + "InputBuffer");
            }

            //Grant write acesses to output data only if it is already mapped,
            //otherwise, we'll have to hook other system functions
            uint64_t mdlAddress = irp.MdlAddress;
            MDL32 mdl;
            if (state->readMemoryConcrete(mdlAddress, &mdl, sizeof(mdl))) {
                if (mdl.MdlFlags & (MDL_MAPPED_TO_SYSTEM_VA | MDL_SOURCE_IS_NONPAGED_POOL)) {
                    m_memoryChecker->grantMemory(state, mdl.MappedSystemVa,
                                                 mdl.Size, MemoryChecker::READWRITE,
                                                 regionType.str() + "InputOutputBuffer");
                }
            }

        }
        break;

        case METHOD_NEITHER: {
            //uint64_t inputBuffer = stackLocation.Parameters.DeviceIoControl.Type3InputBuffer;
            //uint64_t outputBuffer = irp.UserBuffer;

            s2e()->getWarningsStream() << "METHOD_NEITHER not implemented yet...\n";
        }
        break;
    }

#if 0
    if (inputBuffer) {
        for (uint64_t i=0; i<inputLength; ++i) {
            klee::ref<klee::Expr> symb = state->createSymbolicValue(klee::Expr::Int8,
                                         getVariableName(state, __FUNCTION__) + "_IoctlBuffer");
            state->writeMemory(inputBuffer + i, symb);
        }
    }
#endif

    DECLARE_PLUGINSTATE(NtoskrnlHandlersState, state);
    if (consistency>=LOCAL) { // && !plgState->isIoctlIrpExplored) {
        uint32_t pStackLocation = IoGetCurrentIrpStackLocation(&irp);

        plgState->isIoctlIrpExplored = true;

        //Fork one state that will run unmodified and one fake state with symbolic ioctl code
        std::vector<S2EExecutionState *> states;
        forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_fakeioctl");

        S2EExecutionState *realState = states[0] == state ? states[1] : states[0];
        S2EExecutionState *fakeState = state;

        klee::ref<klee::Expr> symb = fakeState->createSymbolicValue(
                    getVariableName(state, __FUNCTION__) + "_IoctlCode", klee::Expr::Int32);

        symb = klee::OrExpr::create(symb, klee::ConstantExpr::create(ioctl & 3, klee::Expr::Int32));
        fakeState->writeMemory(pStackLocation + ioctlOffset, symb);

        FUNCMON_REGISTER_RETURN_A(fakeState, m_functionMonitor, NtoskrnlHandlers::DriverDispatchRet, IRP_MJ_DEVICE_CONTROL, true, pIrp);
        FUNCMON_REGISTER_RETURN_A(realState, m_functionMonitor, NtoskrnlHandlers::DriverDispatchRet, IRP_MJ_DEVICE_CONTROL, false, pIrp);
    } else {
        s2e()->getDebugStream() << "Already explored or <LOCAL consistency\n";

        FUNCMON_REGISTER_RETURN_A(state, m_functionMonitor, NtoskrnlHandlers::DriverDispatchRet, IRP_MJ_DEVICE_CONTROL, false, pIrp);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::DriverDispatch(S2EExecutionState* state, FunctionMonitorState *fns, uint32_t irpMajor)
{
    HANDLER_TRACE_CALL();
    s2e()->getMessagesStream() << "IRP " << irpMajor << " " << s_irpMjArray[irpMajor] << '\n';
    state->undoCallAndJumpToSymbolic();

    //Read the parameters
    uint32_t pDeviceObject = 0;
    uint32_t pIrp = 0;

    if (!readConcreteParameter(state, 0, &pDeviceObject)) {
        HANDLER_TRACE_PARAM_FAILED("pDeviceObject");
        return;
    }

    if (!readConcreteParameter(state, 1, &pIrp)) {
        HANDLER_TRACE_PARAM_FAILED("pIrp");
        return;
    }

    //Grant access to the parameters
    if (m_memoryChecker) {
        grantAccessToIrp(state, pIrp);
    }

    IRP irp;
    if (!pIrp || !state->readMemoryConcrete(pIrp, &irp, sizeof(irp))) {
        HANDLER_TRACE_PARAM_FAILED("irp");
        return;
    }

    uint32_t pStackLocation = IoGetCurrentIrpStackLocation(&irp);
    IO_STACK_LOCATION stackLocation;

    if (!pStackLocation || !state->readMemoryConcrete(pStackLocation, &stackLocation, sizeof(stackLocation))) {
        HANDLER_TRACE_PARAM_FAILED("pStackLocation");
        return;
    }

    switch (irpMajor) {
        case IRP_MJ_DEVICE_CONTROL:
            DispatchIoctl(state, pIrp, irp, stackLocation);
            break;

        case IRP_MJ_WRITE:
            DispatchWrite(state, pIrp, irp, stackLocation);
            break;

        default:
            FUNCMON_REGISTER_RETURN_A(state, m_functionMonitor, NtoskrnlHandlers::DriverDispatchRet, irpMajor, false, pIrp);
            break;
    }

}

void NtoskrnlHandlers::DriverDispatchRet(S2EExecutionState* state, uint32_t irpMajor, bool isFake, uint32_t pIrp)
{
    HANDLER_TRACE_RETURN();

    //Get the return value
    klee::ref<klee::Expr> result = state->readCpuRegister(offsetof(CPUX86State, regs[R_EAX]), klee::Expr::Int32);

    if (isFake) {
        std::stringstream ss;
        ss << "Killing faked " << __FUNCTION__ << " " << s_irpMjArray[irpMajor];
        s2e()->getExecutor()->terminateStateEarly(*state, ss.str());
    }

    if (m_memoryChecker) {
        m_memoryChecker->revokeMemoryForModule(state, "args:DispatchDeviceControl:*");
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

//This is a FASTCALL function!
void NtoskrnlHandlers::IofCompleteRequest(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    if (m_memoryChecker) {
        uint32_t pIrp;

        if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_ECX]), &pIrp, sizeof(pIrp))) {
            HANDLER_TRACE_PARAM_FAILED("pIrp");
            return;
        }

        revokeAccessToIrp(state, pIrp);
    }
}

void NtoskrnlHandlers::grantAccessToIrp(S2EExecutionState *state, uint32_t pIrp)
{
    if (!m_memoryChecker) {
        return;
    }

    m_memoryChecker->grantMemoryForModule(state, pIrp, sizeof(IRP),
                                          MemoryChecker::READWRITE, "irp");

    IRP irp;
    if (!pIrp || !state->readMemoryConcrete(pIrp, &irp, sizeof(irp))) {
        HANDLER_TRACE_PARAM_FAILED("irp");
        return;
    }

    uint32_t pStackLocation = IoGetCurrentIrpStackLocation(&irp);

    m_memoryChecker->grantMemoryForModule(state, pStackLocation, sizeof(IO_STACK_LOCATION),
                                          MemoryChecker::READWRITE, "irp-stack-location");

    IO_STACK_LOCATION StackLocation;
    if (!state->readMemoryConcrete(pStackLocation, &StackLocation, sizeof(StackLocation))) {
        HANDLER_TRACE_PARAM_FAILED("stacklocation");
        return;
    }

    m_memoryChecker->grantMemoryForModule(state, StackLocation.FileObject, sizeof(FILE_OBJECT32),
                                          MemoryChecker::READWRITE, "file-object");

}


void NtoskrnlHandlers::revokeAccessToIrp(S2EExecutionState *state, uint32_t pIrp)
{
    if (!m_memoryChecker) {
        return;
    }

    uint64_t callee = state->getTb()->pcOfLastInstr;
    const ModuleDescriptor *desc = m_detector->getModule(state, callee);
    assert(desc && "There must be a module");

    if (!m_memoryChecker->revokeMemoryByPointerForModule(state, desc, pIrp, "irp")) {
        s2e()->getExecutor()->terminateStateEarly(*state, "Could not revoke permissions");
        return;
    }

    IRP irp;
    if (!pIrp || !state->readMemoryConcrete(pIrp, &irp, sizeof(irp))) {
        HANDLER_TRACE_PARAM_FAILED("irp");
        return;
    }

    uint32_t pStackLocation = IoGetCurrentIrpStackLocation(&irp);

    m_memoryChecker->revokeMemoryByPointerForModule(state, desc, pStackLocation, "irp-stack-location");

    IO_STACK_LOCATION StackLocation;
    if (!state->readMemoryConcrete(pStackLocation, &StackLocation, sizeof(StackLocation))) {
        HANDLER_TRACE_PARAM_FAILED("stacklocation");
        return;
    }

    m_memoryChecker->revokeMemoryByPointerForModule(state, desc, StackLocation.FileObject, "file-object");
}


}
}
