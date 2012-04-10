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

#include "NdisHandlers.h"
#include "Ndis.h"

namespace s2e {
namespace plugins {

using namespace windows;

void NdisHandlers::NdisRegisterProtocol(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    state->undoCallAndJumpToSymbolic();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    //Extract the function pointers from the passed data structure
    uint32_t pProtocol, pStatus;
    if (!readConcreteParameter(state, 2, &pProtocol)) {
        return;
    }
    if (!readConcreteParameter(state, 0, &pStatus)) {
        return;
    }


    s2e()->getMessagesStream() << "NDIS_PROTOCOL_CHARACTERISTICS @" << hexval(pProtocol) << '\n';

    s2e::windows::NDIS_PROTOCOL_CHARACTERISTICS32 Protocol;
    if (!state->readMemoryConcrete(pProtocol, &Protocol, sizeof(Protocol))) {
        HANDLER_TRACE_FCNFAILED();
        return;
    }

    //Register each handler
    NDIS_REGISTER_ENTRY_POINT(Protocol.OpenAdapterCompleteHandler, OpenAdapterCompleteHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.CloseAdapterCompleteHandler, CloseAdapterCompleteHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.SendCompleteHandler, SendCompleteHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.TransferDataCompleteHandler, TransferDataCompleteHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.ResetCompleteHandler, ResetCompleteHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.RequestCompleteHandler, RequestCompleteHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.ReceiveHandler, ReceiveHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.ReceiveCompleteHandler, ReceiveCompleteHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.StatusHandler, StatusHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.StatusCompleteHandler, StatusCompleteHandler);

    NDIS_REGISTER_ENTRY_POINT(Protocol.ReceivePacketHandler, ReceivePacketHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.BindAdapterHandler, BindAdapterHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.UnbindAdapterHandler, UnbindAdapterHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.PnPEventHandler, PnPEventHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.UnloadHandler, UnloadHandler);

    NDIS_REGISTER_ENTRY_POINT(Protocol.CoSendCompleteHandler, CoSendCompleteHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.CoStatusHandler, CoStatusHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.CoReceivePacketHandler, CoReceivePacketHandler);
    NDIS_REGISTER_ENTRY_POINT(Protocol.CoAfRegisterNotifyHandler, CoAfRegisterNotifyHandler);

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    klee::ref<klee::Expr> symb;
    if (consistency == OVERAPPROX) {
        symb = createFailure(state, getVariableName(state, __FUNCTION__) + "_result");
    }else {
        std::vector<uint32_t> vec;
        vec.push_back(NDIS_STATUS_RESOURCES);
        vec.push_back(NDIS_STATUS_BAD_CHARACTERISTICS);
        vec.push_back(NDIS_STATUS_BAD_VERSION);
        symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
    }
    state->writeMemory(pStatus, symb);

    //Skip the call in the current state
    state->bypassFunction(4);
    incrementFailures(state);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
VOID ProtocolOpenAdapterComplete(
  __in  NDIS_HANDLE ProtocolBindingContext,
  __in  NDIS_STATUS Status,
  __in  NDIS_STATUS OpenErrorStatus
)
*/
void NdisHandlers::OpenAdapterCompleteHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();

    //Read the status and grant access to the handle
    uint32_t ProtocolBindingContext = 0;
    uint32_t Status = 0;

    readConcreteParameter(state, 0, &ProtocolBindingContext);
    readConcreteParameter(state, 1, &Status);

    s2e()->getDebugStream() << "ProtocolBindingContext=" << hexval(ProtocolBindingContext)
                            << " Status=" << hexval(Status) << "\n";

    if (NT_SUCCESS(Status)) {
        incrementSuccesses(state);
    } else {
        incrementFailures(state);
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::OpenAdapterCompleteHandlerRet)
}

void NdisHandlers::OpenAdapterCompleteHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::CloseAdapterCompleteHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::CloseAdapterCompleteHandlerRet)
}

void NdisHandlers::CloseAdapterCompleteHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::SendCompleteHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::SendCompleteHandlerRet)
}

void NdisHandlers::SendCompleteHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::TransferDataCompleteHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::TransferDataCompleteHandlerRet)
}

void NdisHandlers::TransferDataCompleteHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ResetCompleteHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::ResetCompleteHandlerRet)
}

void NdisHandlers::ResetCompleteHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
VOID ProtocolRequestComplete(
  NDIS_HANDLE ProtocolBindingContext,
  PNDIS_REQUEST NdisRequest,
  NDIS_STATUS Status
);
*/
void NdisHandlers::RequestCompleteHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();

    //Read the status and grant access to the handle
    uint32_t ProtocolBindingContext = 0;
    uint32_t Status = 0;
    uint32_t pNdisRequest = 0;

    readConcreteParameter(state, 0, &ProtocolBindingContext);
    readConcreteParameter(state, 1, &pNdisRequest);
    readConcreteParameter(state, 2, &Status);

    s2e()->getDebugStream() << "ProtocolBindingContext=" << hexval(ProtocolBindingContext)
                            << " pNdisRequest=" << hexval(pNdisRequest) << "\n"
                            << " Status=" << hexval(Status) << "\n";


    if (NT_SUCCESS(Status)) {
        incrementSuccesses(state);
    } else {
        incrementFailures(state);
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::RequestCompleteHandlerRet)
}

void NdisHandlers::RequestCompleteHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ReceiveHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();

    bool pushed = changeConsistencyForEntryPoint(state, STRICT, 0);
    incrementEntryPoint(state);

    FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::ReceiveHandlerRet, pushed);
}

void NdisHandlers::ReceiveHandlerRet(S2EExecutionState* state, bool pushed)
{
    HANDLER_TRACE_RETURN();
    if (pushed) {
        m_models->pop(state);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ReceiveCompleteHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::ReceiveCompleteHandlerRet)
}

void NdisHandlers::ReceiveCompleteHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::StatusHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::StatusHandlerRet)
}

void NdisHandlers::StatusHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::StatusCompleteHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::StatusCompleteHandlerRet)
}

void NdisHandlers::StatusCompleteHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ReceivePacketHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::ReceivePacketHandlerRet)
}

void NdisHandlers::ReceivePacketHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//VOID ProtocolBindAdapter(
//  PNDIS_STATUS Status, NDIS_HANDLE BindContext, PNDIS_STRING DeviceName,
//  PVOID SystemSpecific1, PVOID SystemSpecific2);

void NdisHandlers::BindAdapterHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();

    uint32_t pDeviceName;
    uint32_t pStatus;
    if (!readConcreteParameter(state, 2, &pDeviceName)) {
        HANDLER_TRACE_PARAM_FAILED("DeviceName");
        return;
    }

    if (!readConcreteParameter(state, 0, &pStatus)) {
        HANDLER_TRACE_PARAM_FAILED("Status");
        return;
    }

    //Grant access to the parameters
    if (m_memoryChecker) {
        grantAccessToUnicodeString(state,
                                   pDeviceName,
                                   m_memoryChecker->getRegionTypePrefix(state, "args:BindAdapterHandler:DeviceName"));
    }

    std::string deviceName;
    if (!ReadUnicodeString(state, pDeviceName, deviceName)) {
        HANDLER_TRACE_PARAM_FAILED("DeviceName");
        return;
    }

    s2e()->getMessagesStream() << "BindAdapterHandler: DeviceName=" << deviceName << '\n';


    bool pushed = changeConsistencyForEntryPoint(state, STRICT, 0);
    incrementEntryPoint(state);

    FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::BindAdapterHandlerRet, pStatus, pushed)
}

void NdisHandlers::BindAdapterHandlerRet(S2EExecutionState* state, uint32_t pStatus, bool pushed)
{
    HANDLER_TRACE_RETURN();

    if (pushed) {
        m_models->pop(state);
    }

    uint32_t Status;
    if (state->readMemoryConcrete(pStatus, &Status, sizeof(Status))) {
        if (NT_SUCCESS(Status)) {
            incrementSuccesses(state);
        } else {
            incrementFailures(state);
        }
    }

    if (m_memoryChecker) {
        m_memoryChecker->revokeMemoryForModule(state, "args:BindAdapterHandler:*");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::UnbindAdapterHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();

    uint32_t pStatus;
    if (!readConcreteParameter(state, 0, &pStatus)) {
        HANDLER_TRACE_PARAM_FAILED("Status");
        return;
    }

    FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::UnbindAdapterHandlerRet, pStatus)
}

void NdisHandlers::UnbindAdapterHandlerRet(S2EExecutionState* state, uint32_t pStatus)
{
    HANDLER_TRACE_RETURN();

    uint32_t Status;
    if (state->readMemoryConcrete(pStatus, &Status, sizeof(Status))) {
        if (NT_SUCCESS(Status)) {
            incrementSuccesses(state);
        } else {
            incrementFailures(state);
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::PnPEventHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();

    bool pushed = changeConsistencyForEntryPoint(state, STRICT, 0);
    incrementEntryPoint(state);

    FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::PnPEventHandlerRet, pushed)
}

void NdisHandlers::PnPEventHandlerRet(S2EExecutionState* state, bool pushed)
{
    HANDLER_TRACE_RETURN();

    if (pushed) {
        m_models->pop(state);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::UnloadHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::UnloadHandlerRet)
}

void NdisHandlers::UnloadHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::CoSendCompleteHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::CoSendCompleteHandlerRet)
}

void NdisHandlers::CoSendCompleteHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::CoStatusHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::CoStatusHandlerRet)
}

void NdisHandlers::CoStatusHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::CoReceivePacketHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::CoReceivePacketHandlerRet)
}

void NdisHandlers::CoReceivePacketHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::CoAfRegisterNotifyHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::CoAfRegisterNotifyHandlerRet)
}

void NdisHandlers::CoAfRegisterNotifyHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

}
}
