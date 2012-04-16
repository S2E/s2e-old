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
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>
#include <s2e/Plugins/MemoryChecker.h>
#include <klee/Solver.h>

#include <iostream>
#include <sstream>

using namespace s2e::windows;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(NdisHandlers, "Basic collection of NDIS API functions.", "NdisHandlers",
                  "FunctionMonitor", "Interceptor", "ModuleExecutionDetector", "SymbolicHardware",
                  "ConsistencyModels");


//This maps exported NDIS functions to their handlers
const NdisHandlers::AnnotationsArray NdisHandlers::s_handlers[] = {

    DECLARE_EP_STRUC(NdisHandlers, NdisInitializeWrapper),
    DECLARE_EP_STRUC(NdisHandlers, NdisAllocateBuffer),
    DECLARE_EP_STRUC(NdisHandlers, NdisAllocateBufferPool),
    DECLARE_EP_STRUC(NdisHandlers, NdisAllocateMemory),
    DECLARE_EP_STRUC(NdisHandlers, NdisAllocateMemoryWithTag),
    DECLARE_EP_STRUC(NdisHandlers, NdisAllocateMemoryWithTagPriority),
    DECLARE_EP_STRUC(NdisHandlers, NdisAllocatePacket),
    DECLARE_EP_STRUC(NdisHandlers, NdisAllocatePacketPool),
    DECLARE_EP_STRUC(NdisHandlers, NdisAllocatePacketPoolEx),

    DECLARE_EP_STRUC(NdisHandlers, NdisCloseConfiguration),
    DECLARE_EP_STRUC(NdisHandlers, NdisFreeMemory),
    DECLARE_EP_STRUC(NdisHandlers, NdisFreePacket),
    DECLARE_EP_STRUC(NdisHandlers, NdisFreePacketPool),
    DECLARE_EP_STRUC(NdisHandlers, NdisFreeBuffer),
    DECLARE_EP_STRUC(NdisHandlers, NdisFreeBufferPool),

    DECLARE_EP_STRUC(NdisHandlers, NdisMAllocateMapRegisters),
    DECLARE_EP_STRUC(NdisHandlers, NdisMAllocateSharedMemory),

    DECLARE_EP_STRUC(NdisHandlers, NdisMFreeSharedMemory),
    DECLARE_EP_STRUC(NdisHandlers, NdisMInitializeTimer),
    DECLARE_EP_STRUC(NdisHandlers, NdisMMapIoSpace),
    DECLARE_EP_STRUC(NdisHandlers, NdisMQueryAdapterInstanceName),
    DECLARE_EP_STRUC(NdisHandlers, NdisMQueryAdapterResources),
    DECLARE_EP_STRUC(NdisHandlers, NdisMRegisterAdapterShutdownHandler),
    DECLARE_EP_STRUC(NdisHandlers, NdisMRegisterInterrupt),
    DECLARE_EP_STRUC(NdisHandlers, NdisMRegisterIoPortRange),
    DECLARE_EP_STRUC(NdisHandlers, NdisMRegisterMiniport),
    DECLARE_EP_STRUC(NdisHandlers, NdisMSetAttributes),
    DECLARE_EP_STRUC(NdisHandlers, NdisMSetAttributesEx),

    DECLARE_EP_STRUC(NdisHandlers, NdisOpenAdapter),
    DECLARE_EP_STRUC(NdisHandlers, NdisOpenConfiguration),
    DECLARE_EP_STRUC(NdisHandlers, NdisQueryAdapterInstanceName),
    DECLARE_EP_STRUC(NdisHandlers, NdisQueryPendingIOCount),


    DECLARE_EP_STRUC(NdisHandlers, NdisReadConfiguration),
    DECLARE_EP_STRUC(NdisHandlers, NdisReadNetworkAddress),
    DECLARE_EP_STRUC(NdisHandlers, NdisReadPciSlotInformation),
    DECLARE_EP_STRUC(NdisHandlers, NdisRegisterProtocol),
    DECLARE_EP_STRUC(NdisHandlers, NdisSetTimer),
    DECLARE_EP_STRUC(NdisHandlers, NdisWriteErrorLogEntry),
    DECLARE_EP_STRUC(NdisHandlers, NdisWritePciSlotInformation),
};

const char *NdisHandlers::s_ignoredFunctionsList[] = {
    "NdisCancelSendPackets",
    "NdisCloseAdapter",
    "NdisCopyFromPacketToPacket",
    "NdisGeneratePartialCancelId",
    "NdisInitializeEvent",
    "NdisReturnPackets",
    "NdisSetEvent",
    "NdisUnchainBufferAtFront",
    "NdisWaitEvent",

    //XXX: Revoke rights for these
    "NdisDeregisterProtocol",

    NULL
};

//XXX: Implement these
//NdisQueryAdapterInstanceName, NdisQueryPendingIOCount
//NdisRequest

const SymbolDescriptor NdisHandlers::s_exportedVariablesList[] = {
    {"", 0}
};

const NdisHandlers::AnnotationsMap NdisHandlers::s_handlersMap =
        NdisHandlers::initializeHandlerMap();

const NdisHandlers::StringSet NdisHandlers::s_ignoredFunctions =
        NdisHandlers::initializeIgnoredFunctionSet();

const SymbolDescriptors NdisHandlers::s_exportedVariables =
        NdisHandlers::initializeExportedVariables();


void NdisHandlers::initialize()
{
    ConfigFile *cfg = s2e()->getConfig();

    WindowsApi::initialize();

    m_hw = static_cast<SymbolicHardware*>(s2e()->getPlugin("SymbolicHardware"));


    bool ok;
    m_devDesc = NULL;
    m_hwId = cfg->getString(getConfigKey() + ".hwId", "", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "NDISHANDLERS: You did not configure any symbolic hardware id" << '\n';
    }else {
        m_devDesc = m_hw->findDevice(m_hwId);
        if (!m_devDesc) {
            s2e()->getWarningsStream() << "NDISHANDLERS: The specified hardware device id is invalid " << m_hwId << '\n';
            exit(-1);
        }
    }

    //Checking the keywords for NdisReadConfiguration whose result will not be replaced with symbolic values
    ConfigFile::string_list ign = cfg->getStringList(getConfigKey() + ".ignoreKeywords");
    m_ignoreKeywords.insert(ign.begin(), ign.end());

    //The multiplicative interval factor slows down the frequency of the registered timers
    //via NdisSetTimer.
    m_timerIntervalFactor = cfg->getInt(getConfigKey() + ".timerIntervalFactor", 1, &ok);

    //Read the hard-coded mac address, in case we do not want it to be symbolic
    ConfigFile::integer_list mac = cfg->getIntegerList(getConfigKey() + ".networkAddress",
                                                       ConfigFile::integer_list(), &ok);
    if (ok && mac.size() > 0) {
        foreach2(it, mac.begin(), mac.end()) {
            m_networkAddress.push_back(*it);
        }
    }

    //What device type do we want to force?
    //This is only for overapproximate consistency
    m_forceAdapterType = cfg->getInt(getConfigKey() + ".forceAdapterType", InterfaceTypeUndefined, &ok);

    //For debugging: generate a crash dump when the driver is loaded
    m_generateDumpOnLoad = cfg->getBool(getConfigKey() + ".generateDumpOnLoad", false, &ok);
    if (m_generateDumpOnLoad) {
        m_crashdumper = static_cast<WindowsCrashDumpGenerator*>(s2e()->getPlugin("WindowsCrashDumpGenerator"));
        if (!m_crashdumper) {
            s2e()->getWarningsStream() << "NDISHANDLERS: generateDumpOnLoad option requires the WindowsCrashDumpGenerator plugin!"
                    << '\n';
            exit(-1);
        }
    }



    m_windowsMonitor->onModuleUnload.connect(
            sigc::mem_fun(*static_cast<WindowsApi*>(this),
                    &WindowsApi::onModuleUnload)
            );

    ASSERT_STRUC_SIZE(NDIS_PROTOCOL_CHARACTERISTICS32, NDIS_PROTOCOL_CHARACTERISTICS_SIZE)
    ASSERT_STRUC_SIZE(NDIS_PROTOCOL_BLOCK32, NDIS_PROTOCOL_BLOCK_SIZE)
    ASSERT_STRUC_SIZE(NDIS_COMMON_OPEN_BLOCK32, NDIS_COMMON_OPEN_BLOCK_SIZE)
    ASSERT_STRUC_SIZE(NDIS_OPEN_BLOCK32, NDIS_OPEN_BLOCK_SIZE)
}


////////////////////////////////////////////////////////////////////////////////////////////////////////

void NdisHandlers::NdisInitializeWrapper(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    FUNCMON_REGISTER_RETURN_A(state, m_functionMonitor, NdisHandlers::NdisInitializeWrapperRet, 0);

}

void NdisHandlers::NdisInitializeWrapperRet(S2EExecutionState* state, uint32_t pHandle)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

void NdisHandlers::NdisAllocateMemoryWithTag(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();
    NdisAllocateMemoryBase(state, fns);
}

void NdisHandlers::NdisAllocateMemoryWithTagRet(S2EExecutionState* state, uint32_t Address, uint32_t Length)
{
    //Call the normal allocator annotation, since both functions are similar
    NdisAllocateMemoryRet(state, Address, Length);
}

void NdisHandlers::NdisAllocateMemory(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    NdisAllocateMemoryBase(state, fns);
}

void NdisHandlers::NdisAllocateMemoryBase(S2EExecutionState* state, FunctionMonitorState *fns)
{
    state->undoCallAndJumpToSymbolic();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    bool ok = true;
    uint32_t Address, Length;
    ok &= readConcreteParameter(state, 0, &Address);
    ok &= readConcreteParameter(state, 1, &Length);
    if(!ok) {
        s2e()->getDebugStream(state) << "Can not read address and length of memory allocation" << '\n';
        return;
    }

    if (consistency < LOCAL) {
        //We'll have to grant access to the memory array
        FUNCMON_REGISTER_RETURN_A(state, m_functionMonitor, NdisHandlers::NdisAllocateMemoryRet, Address, Length);
        return;
    }

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__)+"_failure");

    //Skip the call in the current state
    state->bypassFunction(3);

    //The doc also specifies that the address must be null in case of a failure
    uint32_t null = 0;
    bool suc = state->writeMemoryConcrete(Address, &null, sizeof(null));
    assert(suc);

    uint32_t failValue = 0xC0000001;
    state->writeCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &failValue, sizeof(failValue));

    incrementFailures(state);

    //Register the return handler
    S2EExecutionState *otherState = states[0] == state ? states[1] : states[0];
    FUNCMON_REGISTER_RETURN_A(otherState, m_functionMonitor, NdisHandlers::NdisAllocateMemoryRet, Address, Length);
}

void NdisHandlers::NdisAllocateMemoryRet(S2EExecutionState* state, uint32_t Address, uint32_t Length)
{
    HANDLER_TRACE_RETURN();
    state->jumpToSymbolicCpp();

    //Get the return value
    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &eax, sizeof(eax))) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": return status is not concrete" << '\n';
        return;
    }

    //The original function has failed
    if (eax) {
        HANDLER_TRACE_FCNFAILED();
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);

    if(m_memoryChecker) {
        uint32_t BufAddress;
        bool ok = state->readMemoryConcrete(Address, &BufAddress, 4);
        if(!ok) {
            s2e()->getWarningsStream() << __FUNCTION__ << ": cannot read allocated address" << '\n';
            return;
        }
        m_memoryChecker->grantMemory(state, BufAddress, Length, MemoryChecker::READWRITE,
                                     "ndis:alloc:NdisAllocateMemory", BufAddress);
    }
}

void NdisHandlers::NdisAllocateMemoryWithTagPriority(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    bool ok = true;
    uint32_t Length;
    ok &= readConcreteParameter(state, 1, &Length);
    if(!ok) {
        s2e()->getDebugStream(state) << __FUNCTION__ << ": can not read address params" << '\n';
        return;
    }
    FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::NdisAllocateMemoryWithTagPriorityRet, Length);
}

void NdisHandlers::NdisAllocateMemoryWithTagPriorityRet(S2EExecutionState* state, uint32_t Length)
{
    HANDLER_TRACE_RETURN();

    if(m_memoryChecker) {
        //Get the return value
        uint32_t eax;
        if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &eax, sizeof(eax))) {
            s2e()->getWarningsStream() << __FUNCTION__  << ": return status is not concrete" << '\n';
            return;
        }

        if (!eax) {
            //The original function has failed
            incrementFailures(state);
            return;
        }

        incrementSuccesses(state);
        m_memoryChecker->grantMemory(state, eax, Length,
                                     MemoryChecker::READWRITE,
                                     "ndis:alloc:NdisAllocateMemoryWithTagPriority", eax);
    }
}

void NdisHandlers::NdisFreeMemory(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    if(!m_memoryChecker) {
        return;
    }

    bool ok = true;
    uint32_t Address, Length;
    ok &= readConcreteParameter(state, 0, &Address);
    ok &= readConcreteParameter(state, 1, &Length);
    if(!ok) {
        s2e()->getWarningsStream() << __FUNCTION__ << ": can not read params" << '\n';
    }

    uint64_t AllocatedAddress, AllocatedLength;

    if (!m_memoryChecker->findMemoryRegion(state, Address, &AllocatedAddress, &AllocatedLength)) {
        s2e()->getExecutor()->terminateStateEarly(*state, "NdisFreeMemory: Tried to free an unallocated memory region");
    }

    if (AllocatedLength != Length) {
        std::stringstream ss;
        ss << "NdisFreeMemory called with length=0x" << std::hex << Length
           << " but was allocated 0x" << AllocatedLength;

        warning(state, ss.str());
    }

    m_memoryChecker->revokeMemoryByPointer(state, Address, "");
}

void NdisHandlers::NdisMFreeSharedMemory(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    bool ok = true;

    uint32_t physAddr;
    uint32_t virtAddr;
    uint32_t length;

    //XXX: Physical address should be 64 bits
    ok &= readConcreteParameter(state, 1, &length); //Length
    ok &= readConcreteParameter(state, 3, &virtAddr); //VirtualAddress
    ok &= readConcreteParameter(state, 4, &physAddr); //PhysicalAddress

    if (!ok) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": could not read parameters" << '\n';
        return;
    }

    m_hw->resetSymbolicMmioRange(state, physAddr, length);

    if(m_memoryChecker) {
        m_memoryChecker->revokeMemory(state, virtAddr, length);
    }
}


void NdisHandlers::NdisMAllocateSharedMemory(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    bool ok = true;
    uint32_t Length, VirtualAddress, PhysicalAddress;


    ok &= readConcreteParameter(state, 1, &Length); //Length
    ok &= readConcreteParameter(state, 3, &VirtualAddress); //VirtualAddress
    ok &= readConcreteParameter(state, 4, &PhysicalAddress); //PhysicalAddress

    if (!ok) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": could not read parameters" << '\n';
        return;
    }

    if (consistency < LOCAL) {
        FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::NdisMAllocateSharedMemoryRet, Length, VirtualAddress,
                                  PhysicalAddress);
        return;
    }

    state->undoCallAndJumpToSymbolic();


    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Skip the call in the current state
    state->bypassFunction(5);

    //Fail the function call
    uint32_t null = 0;
    state->writeMemoryConcrete(VirtualAddress, &null, sizeof(null));
    incrementFailures(state);

    FUNCMON_REGISTER_RETURN_A(state == states[0] ? states[1] : states[0],
                            m_functionMonitor, NdisHandlers::NdisMAllocateSharedMemoryRet,
                            Length, VirtualAddress, PhysicalAddress);
}

void NdisHandlers::NdisMAllocateSharedMemoryRet(S2EExecutionState* state,
                                                uint32_t Length, uint32_t pVirtualAddress, uint32_t pPhysicalAddress)
{
    HANDLER_TRACE_RETURN();


    bool ok=true;
    uint32_t va = 0;
    uint64_t pa = 0;

    ok &= state->readMemoryConcrete(pVirtualAddress, &va, sizeof(va));
    ok &= state->readMemoryConcrete(pPhysicalAddress, &pa, sizeof(pa));
    if (!ok) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": could not read returned addresses" << '\n';
        s2e()->getWarningsStream() << "VirtualAddress=" << hexval(va) << " PhysicalAddress=" << hexval(pa) << '\n';
        return;
    }

    if (!va) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": original call has failed" << '\n';
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);

    //Register symbolic DMA memory.
    //All reads from it will be symbolic.
    m_hw->setSymbolicMmioRange(state, pa, Length);

    if(m_memoryChecker) {
        m_memoryChecker->grantMemory(state, va, Length,
                                     MemoryChecker::READWRITE,
                                     "ndis:hw:NdisMAllocateSharedMemory");
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//XXX: Fork a success and a failure!
void NdisHandlers::NdisAllocatePacket(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    uint32_t pStatus, pPacket;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &pStatus);
    ok &= readConcreteParameter(state, 1, &pPacket);

    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters" << '\n';
        return;
    }

    FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::NdisAllocatePacketRet, pStatus, pPacket);
}

void NdisHandlers::NdisAllocatePacketRet(S2EExecutionState* state, uint32_t pStatus, uint32_t pPacket)
{
    HANDLER_TRACE_RETURN();

    bool ok = true;
    NDIS_STATUS Status;
    uint32_t Packet;

    ok &= state->readMemoryConcrete(pStatus, &Status, sizeof(Status));
    ok &= state->readMemoryConcrete(pPacket, &Packet, sizeof(Packet));
    if(!ok) {
        s2e()->getDebugStream() << "Can not read result" << '\n';
        return;
    }

    if(!NT_SUCCESS(Status)) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": original call has failed\n";
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);
    if(m_memoryChecker) {
        DECLARE_PLUGINSTATE(NdisHandlersState, state);
        uint32_t size = sizeof(NDIS_PACKET32) + sizeof(NDIS_PACKET_OOB_DATA32) + sizeof(NDIS_PACKET_EXTENSION32) +
        plgState->ProtocolReservedLength;
        m_memoryChecker->grantMemory(state, Packet, size,
                                     MemoryChecker::READWRITE,
                                     "ndis:alloc:NdisAllocatePacket");
    }
}

void NdisHandlers::NdisFreePacket(S2EExecutionState *state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    uint32_t packet;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &packet);

    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters" << '\n';
        return;
    }

    revokePacket(state, packet);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisAllocateBufferPool(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    uint32_t pStatus, pPoolHandle;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &pStatus);
    ok &= readConcreteParameter(state, 1, &pPoolHandle);
    if (!ok) {
        HANDLER_TRACE_FCNFAILED();
        return;
    }

    if (consistency < LOCAL) {
        FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::NdisAllocateBufferPoolRet, pStatus, pPoolHandle);
        return;
    }

    state->undoCallAndJumpToSymbolic();


    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Skip the call in the current state
    state->bypassFunction(3);

    //Write symbolic status code
    state->writeMemory(pStatus, createFailure(state, getVariableName(state, __FUNCTION__) + "_result"));

    incrementFailures(state);

    FUNCMON_REGISTER_RETURN_A(states[0] == state ? states[1] : states[0],
                              m_functionMonitor, NdisHandlers::NdisAllocateBufferPoolRet, pStatus, pPoolHandle);
}

void NdisHandlers::NdisAllocateBufferPoolRet(S2EExecutionState* state, uint32_t pStatus, uint32_t pPoolHandle)
{
    HANDLER_TRACE_RETURN();

    bool ok = true;
    NDIS_STATUS Status;
    NDIS_HANDLE Handle;

    ok &= state->readMemoryConcrete(pStatus, &Status, sizeof(Status));
    ok &= state->readMemoryConcrete(pPoolHandle, &Handle, sizeof(Handle));
    if(!ok) {
        s2e()->getDebugStream() << "Can not read result\n";
        return;
    }

    if(!NT_SUCCESS(Status)) {
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);
    if(m_memoryChecker) {
        //The handle is NULL on some versions of Windows (no-op)
        if (Handle != 0) {
            m_memoryChecker->grantResource(state, Handle, "ndis:alloc:NdisAllocateBufferPool");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisAllocatePacketPool(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    uint32_t pStatus, pPoolHandle, ProtocolReservedLength;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &pStatus);
    ok &= readConcreteParameter(state, 1, &pPoolHandle);
    ok &= readConcreteParameter(state, 3, &ProtocolReservedLength);
    if (!ok) {
        HANDLER_TRACE_FCNFAILED();
        return;
    }

    if (consistency < LOCAL) {
        FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::NdisAllocatePacketPoolRet, pStatus, pPoolHandle,
                                  ProtocolReservedLength);
        return;
    }

    state->undoCallAndJumpToSymbolic();


    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    klee::ref<klee::Expr> symb;
    if (consistency == OVERAPPROX) {
        symb = createFailure(state, getVariableName(state, __FUNCTION__) + "_result");
    }else {
        std::vector<uint32_t> vec;
        vec.push_back(NDIS_STATUS_RESOURCES);
        symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
    }
    state->writeMemory(pStatus, symb);

    //Skip the call in the current state
    state->bypassFunction(4);

    incrementFailures(state);

    FUNCMON_REGISTER_RETURN_A(states[0] == state ? states[1] : states[0],
                              m_functionMonitor, NdisHandlers::NdisAllocatePacketPoolRet, pStatus, pPoolHandle,
                              ProtocolReservedLength);
}

void NdisHandlers::NdisAllocatePacketPoolRet(S2EExecutionState* state, uint32_t pStatus, uint32_t pPoolHandle,
                                             uint32_t ProtocolReservedLength)
{
    HANDLER_TRACE_RETURN();

    bool ok = true;
    NDIS_STATUS Status;
    NDIS_HANDLE Handle;

    ok &= state->readMemoryConcrete(pStatus, &Status, sizeof(Status));
    ok &= state->readMemoryConcrete(pPoolHandle, &Handle, sizeof(Handle));
    if(!ok) {
        s2e()->getDebugStream() << "Can not read result\n";
        return;
    }

    if(!NT_SUCCESS(Status)) {
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);
    if(m_memoryChecker) {
        m_memoryChecker->grantResource(state, Handle, "ndis:alloc:NdisAllocatePacketPool");
    }

    DECLARE_PLUGINSTATE(NdisHandlersState, state);
    plgState->ProtocolReservedLength = ProtocolReservedLength;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisAllocatePacketPoolEx(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    uint32_t pStatus, pPoolHandle;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &pStatus);
    ok &= readConcreteParameter(state, 1, &pPoolHandle);
    if (!ok) {
        HANDLER_TRACE_FCNFAILED();
        return;
    }

    if (consistency < LOCAL) {
        FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::NdisAllocatePacketPoolExRet, pStatus, pPoolHandle);
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Write symbolic status code
    klee::ref<klee::Expr> symb;
    if (consistency == OVERAPPROX) {
        symb = createFailure(state, getVariableName(state, __FUNCTION__) + "_result");
    }else {
        std::vector<uint32_t> vec;
        vec.push_back(NDIS_STATUS_RESOURCES);
        symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), symb);
    }
    state->writeMemory(pStatus, symb);

    //Skip the call in the current state
    state->bypassFunction(5);

    incrementFailures(state);

    FUNCMON_REGISTER_RETURN_A(states[0] == state ? states[1] : states[0],
                              m_functionMonitor, NdisHandlers::NdisAllocatePacketPoolExRet, pStatus, pPoolHandle);
}

void NdisHandlers::NdisAllocatePacketPoolExRet(S2EExecutionState* state, uint32_t pStatus, uint32_t pPoolHandle)
{
    HANDLER_TRACE_RETURN();

    bool ok = true;
    NDIS_STATUS Status;
    NDIS_HANDLE Handle;

    ok &= state->readMemoryConcrete(pStatus, &Status, sizeof(Status));
    ok &= state->readMemoryConcrete(pPoolHandle, &Handle, sizeof(Handle));
    if(!ok) {
        s2e()->getDebugStream() << "Can not read result\n";
        return;
    }

    if(!NT_SUCCESS(Status)) {
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);

    if(m_memoryChecker) {
        m_memoryChecker->grantResource(state, Handle, "ndis:alloc:NdisAllocatePacketPoolEx");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//XXX: Avoid copy/pasting of code with NdisOpenAdapter and other similar functions.
void NdisHandlers::NdisOpenConfiguration(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency < LOCAL) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    uint32_t pStatus;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &pStatus);
    if (!ok) {
        HANDLER_TRACE_FCNFAILED();
        return;
    }

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Write symbolic status code
    klee::ref<klee::Expr> symb;
    if (consistency == OVERAPPROX) {
        symb = createFailure(state, getVariableName(state, __FUNCTION__) + "_result");
    }else {
        std::vector<uint32_t> vec;
        //This is a success code. Might cause problems later...
        vec.push_back(NDIS_STATUS_FAILURE);
        symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
    }
    state->writeMemory(pStatus, symb);

    //Skip the call in the current state
    state->bypassFunction(3);

    incrementFailures(state);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisMQueryAdapterInstanceName(S2EExecutionState* state, FunctionMonitorState *fns)
{
    NdisQueryAdapterInstanceName(state, fns);
}

/**
NDIS_STATUS NdisQueryAdapterInstanceName(
  __out  PNDIS_STRING AdapterInstanceName,
  __in   NDIS_HANDLE NdisBindingHandle
);
*/
void NdisHandlers::NdisQueryAdapterInstanceName(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    //Read AdapterInstanceName
    uint32_t pUnicodeString = 0;
    readConcreteParameter(state, 0, &pUnicodeString);

    if (consistency < LOCAL) {
        if (pUnicodeString) {
            FUNCMON_REGISTER_RETURN_A(state, m_functionMonitor, NdisHandlers::NdisQueryAdapterInstanceNameRet, pUnicodeString);
        }else {
            HANDLER_TRACE_PARAM_FAILED(0);
        }
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Write symbolic status code
    klee::ref<klee::Expr> symb;
    if (consistency == OVERAPPROX) {
        symb = createFailure(state, getVariableName(state, __FUNCTION__) + "_result");
    }else {
        std::vector<uint32_t> vec;
        vec.push_back(NDIS_STATUS_RESOURCES);
        symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
    }

    state->writeCpuRegister(CPU_OFFSET(regs[R_EAX]), symb);

    //Skip the call in the current state
    state->bypassFunction(2);
    incrementFailures(state);

    //Register a return handler in the normal state
    S2EExecutionState *successState = states[0] == state ? states[1] : states[0];

    if (pUnicodeString) {
        FUNCMON_REGISTER_RETURN_A(successState, m_functionMonitor, NdisHandlers::NdisQueryAdapterInstanceNameRet, pUnicodeString);
    }else {
        HANDLER_TRACE_PARAM_FAILED(0);
    }
}

void NdisHandlers::NdisQueryAdapterInstanceNameRet(S2EExecutionState* state, uint64_t pUnicodeString)
{
    HANDLER_TRACE_RETURN();
    if (!pUnicodeString) {
        return;
    }

    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &eax, sizeof(eax))) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": return status is not concrete" << '\n';
        return;
    }

    if(eax) {
        s2e()->getDebugStream() << __FUNCTION__ << " failed with " << hexval(eax) << '\n';
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);

    NDIS_STRING s;
    bool ok = true;
    ok = state->readMemoryConcrete(pUnicodeString, &s, sizeof(s));
    if(!ok) {
        s2e()->getDebugStream() << "Can not read NDIS_STRING" << '\n';
        return;
    }

    std::string adapterName;
    if (state->readUnicodeString(s.Buffer, adapterName)) {
        s2e()->getDebugStream() << __FUNCTION__ << ": name " << adapterName << '\n';
    }

    if(m_memoryChecker) {
        m_memoryChecker->grantMemory(state, s.Buffer, s.Length, MemoryChecker::READ,
                             "ndis:ret:NdisMQueryAdapterInstanceName");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisQueryPendingIOCount(S2EExecutionState* state, FunctionMonitorState *fns)
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

    //Write symbolic status code
    klee::ref<klee::Expr> symb;
    if (consistency == OVERAPPROX) {
        symb = createFailure(state, getVariableName(state, __FUNCTION__) + "_result");
    }else {
        std::vector<uint32_t> vec;

        //XXX: check this one...
        vec.push_back(NDIS_STATUS_CLOSING);
        vec.push_back(NDIS_STATUS_FAILURE);
        symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
    }
    state->writeCpuRegister(CPU_OFFSET(regs[R_EAX]), symb);

    //Skip the call in the current state
    state->bypassFunction(2);
    incrementFailures(state);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisOpenAdapter(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency < LOCAL) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    uint32_t pStatus;
    uint32_t pNdisBindingHandle = 0;
    uint32_t ProtocolBindingContext = 0;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &pStatus);
    ok &= readConcreteParameter(state, 2, &pNdisBindingHandle);
    ok &= readConcreteParameter(state, 7, &ProtocolBindingContext);
    if (!ok) {
        HANDLER_TRACE_FCNFAILED();
        return;
    }

    s2e()->getDebugStream() << "ProtocolBindingContext=" << hexval(ProtocolBindingContext) << "\n";

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Write symbolic status code
    klee::ref<klee::Expr> symb;
    if (consistency == OVERAPPROX) {
        symb = createFailure(state, getVariableName(state, __FUNCTION__) + "_result");
    }else {
        std::vector<uint32_t> vec;
        //This is a success code. Might cause problems later...
        //vec.push_back(NDIS_STATUS_PENDING);
        vec.push_back(NDIS_STATUS_RESOURCES);
        vec.push_back(NDIS_STATUS_ADAPTER_NOT_FOUND);
        vec.push_back(NDIS_STATUS_UNSUPPORTED_MEDIA);
        vec.push_back(NDIS_STATUS_CLOSING);
        vec.push_back(NDIS_STATUS_OPEN_FAILED);
        symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
    }
    state->writeMemory(pStatus, symb);


    FUNCMON_REGISTER_RETURN_A(states[0] == state ? states[1] : states[0],
                              m_functionMonitor, NdisHandlers::NdisOpenAdapterRet,
                              pStatus, pNdisBindingHandle);

    //Skip the call in the current state
    state->bypassFunction(11);

    incrementFailures(state);
}

void NdisHandlers::NdisOpenAdapterRet(S2EExecutionState* state,
                                      uint32_t pStatus, uint32_t pNdisBindingHandle)
{
    HANDLER_TRACE_RETURN();

    uint32_t Status, NdisBindingHandle;
    bool ok = true;
    ok &= state->readMemoryConcrete(pStatus, &Status, sizeof(Status));
    ok &= state->readMemoryConcrete(pNdisBindingHandle, &NdisBindingHandle, sizeof(NdisBindingHandle));

    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters\n";
        return;
    }

    if (!NT_SUCCESS(Status)) {
        HANDLER_TRACE_FCNFAILED_VAL(Status);
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);

    if (Status == windows::NDIS_STATUS_PENDING) {
        //Can't grant access rights to the handle yet
        return;
    }

    //TODO: finish access granting
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//XXX: Fork a success and a failure!
void NdisHandlers::NdisAllocateBuffer(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    uint32_t pStatus, pBuffer;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &pStatus);
    ok &= readConcreteParameter(state, 1, &pBuffer);

    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters" << '\n';
        return;
    }

    FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::NdisAllocateBufferRet, pStatus, pBuffer);
}

void NdisHandlers::NdisAllocateBufferRet(S2EExecutionState* state, uint32_t pStatus, uint32_t pBuffer)
{
    HANDLER_TRACE_RETURN();

    bool ok = true;
    NDIS_STATUS Status;
    uint32_t Buffer;

    ok &= state->readMemoryConcrete(pStatus, &Status, sizeof(Status));
    ok &= state->readMemoryConcrete(pBuffer, &Buffer, sizeof(Buffer));
    if(!ok) {
        s2e()->getDebugStream() << "Can not read result" << '\n';
        return;
    }

    if(Status) {
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);

    //Length += 0x1000; // XXX

    if(m_memoryChecker) {
        m_memoryChecker->grantMemory(state, Buffer, sizeof(NDIS_BUFFER32), MemoryChecker::READWRITE,
                                     "ndis:alloc:NdisAllocateBuffer");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisFreeBuffer(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    uint32_t Buffer;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &Buffer);

    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters\n";
        return;
    }

    if(m_memoryChecker) {
        m_memoryChecker->revokeMemory(state, Buffer, sizeof(NDIS_BUFFER32), MemoryChecker::READWRITE);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisFreePacketPool(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    uint32_t Handle;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &Handle);

    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters\n";
        return;
    }

    if(m_memoryChecker) {
        m_memoryChecker->revokeResource(state, Handle);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisFreeBufferPool(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    uint32_t Handle;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &Handle);

    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters\n";
        return;
    }

    if(m_memoryChecker) {
        //The handle is NULL on some versions of Windows (no-op)
        if (Handle) {
            m_memoryChecker->revokeResource(state, Handle);
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisMInitializeTimer(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    uint32_t TimerFunction, Timer;
    if (!readConcreteParameter(state, 0, &Timer)) {
        s2e()->getDebugStream() << "Could not read function pointer for timer entry point\n";
        return;
    }

    if (!readConcreteParameter(state, 2, &TimerFunction)) {
        s2e()->getDebugStream() << "Could not read function pointer for timer entry point\n";
        return;
    }

    uint32_t priv;
    if (!readConcreteParameter(state, 3, &priv)) {
        s2e()->getDebugStream() << "Could not read private pointer for timer entry point\n";
        return;
    }

    s2e()->getDebugStream(state) << "NdisMInitializeTimer pc=" << hexval(TimerFunction) <<
            " priv=" << hexval(priv) << '\n';

    m_timerEntryPoints.insert(std::make_pair(TimerFunction, priv));

    FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::NdisMInitializeTimerRet, Timer, TimerFunction)
}

void NdisHandlers::NdisMInitializeTimerRet(S2EExecutionState* state, uint32_t Timer, uint32_t TimerFunction)
{
    HANDLER_TRACE_RETURN();


    NDIS_REGISTER_ENTRY_POINT(TimerFunction, NdisTimerEntryPoint);
}

//This annotation will try to run all timer entry points at once to maximize coverage.
//This is only for overapproximate consistency.
void NdisHandlers::NdisTimerEntryPoint(S2EExecutionState* state, FunctionMonitorState *fns)
{
    static TimerEntryPoints exploredRealEntryPoints;
    static TimerEntryPoints exploredEntryPoints;
    TimerEntryPoints scheduled;

    state->undoCallAndJumpToSymbolic();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    HANDLER_TRACE_CALL();

    //state->dumpStack(20, state->getSp());

    uint32_t realPc = state->getPc();
    uint32_t realPriv = 0;
    if (!readConcreteParameter(state, 1, &realPriv)) {
        s2e()->getDebugStream(state) << "Could not read value of opaque pointer" << '\n';
        return;
    }
    s2e()->getDebugStream(state) << "realPc=" << hexval(realPc) << " realpriv=" << hexval(realPriv) << '\n';

    if (consistency != OVERAPPROX) {
        FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisTimerEntryPointRet)
        return;
    }


    //If this this entry point is called for real for the first time,
    //schedule it for execution.
    if (exploredRealEntryPoints.find(std::make_pair(realPc, realPriv)) == exploredRealEntryPoints.end()) {
        s2e()->getDebugStream(state) << "Never called for real, schedule for execution" << '\n';
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
    if (scheduled.size() == 0) {
        s2e()->getDebugStream(state) << "No need to redundantly run the timer another time" << '\n';
        state->bypassFunction(4);
        throw CpuExitException();
    }

    //If a timer is scheduled, make sure that the real one is also there.
    //Otherwise, there may be a crash, and all states could wrongly terminate.
    if (scheduled.size() > 0) {
       if (scheduled.find(std::make_pair(realPc, realPriv)) == scheduled.end()) {
           scheduled.insert(std::make_pair(realPc, realPriv));
       }
    }

    //Must register return handler before forking.
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisTimerEntryPointRet)

    //Fork the number of states corresponding to the number of timers
    std::vector<S2EExecutionState*> states;
    forkStates(state, states, scheduled.size() - 1, __FUNCTION__);
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

        s2e()->getDebugStream() << "Found timer " << hexval(pc) << " with private struct " << hexval(priv)
         << " to explore." << '\n';

        //Overwrite the original private field with the new one
        klee::ref<klee::Expr> privExpr = klee::ConstantExpr::create(priv, klee::Expr::Int32);
        curState->writeMemory(physAddr, privExpr, S2EExecutionState::PhysicalAddress);

        //Overwrite the program counter with the new handler
        curState->writeCpuState(offsetof(CPUX86State, eip), pc, sizeof(uint32_t)*8);

        //Mark wheter this state will explore a fake call or a real one.
        DECLARE_PLUGINSTATE(NdisHandlersState, curState);
        plgState->faketimer = !(pc == realPc && priv == realPriv);
    }


}

void NdisHandlers::NdisTimerEntryPointRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    //We must terminate states that ran fake calls, otherwise the system might crash later.
    if (plgState->faketimer) {
        s2e()->getExecutor()->terminateStateEarly(*state, "Terminating state with fake timer call");
    }

    if (plgState->cableStatus == NdisHandlersState::DISCONNECTED) {
        s2e()->getExecutor()->terminateStateEarly(*state, "Terminating state because cable is disconnected");
    }

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisSetTimer(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();


    uint32_t interval;
    if (!readConcreteParameter(state, 1, &interval)) {
        s2e()->getDebugStream() << "Could not read timer interval" << '\n';
        return;
    }

    //We make the interval longer to avoid overloading symbolic execution.
    //This should give more time for the rest of the system to execute.
    s2e()->getDebugStream() << "Setting interval to " << interval*m_timerIntervalFactor
            <<" ms. Was " << interval << " ms." << '\n';

    klee::ref<klee::Expr> newInterval = klee::ConstantExpr::create(interval*m_timerIntervalFactor, klee::Expr::Int32);
    writeParameter(state, 1, newInterval);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisMRegisterAdapterShutdownHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();


    DECLARE_PLUGINSTATE(NdisHandlersState, state);
    if (!readConcreteParameter(state, 2, &plgState->val1)) {
        s2e()->getDebugStream() << "Could not read function pointer for timer entry point" << '\n';
        return;
    }

    plgState->shutdownHandler = plgState->val1;

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMRegisterAdapterShutdownHandlerRet)
}

void NdisHandlers::NdisMRegisterAdapterShutdownHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    NDIS_REGISTER_ENTRY_POINT(plgState->val1, NdisShutdownEntryPoint);
}

void NdisHandlers::NdisShutdownEntryPoint(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisShutdownEntryPointRet)
}

void NdisHandlers::NdisShutdownEntryPointRet(S2EExecutionState* state)
{
    HANDLER_TRACE_CALL();

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisMMapIoSpace(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency == STRICT) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMMapIoSpaceRet)
}

void NdisHandlers::NdisMMapIoSpaceRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    state->jumpToSymbolicCpp();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency == LOCAL) {
        std::vector<uint32_t> values;

        values.push_back(NDIS_STATUS_SUCCESS);
        values.push_back(NDIS_STATUS_RESOURCE_CONFLICT);
        values.push_back(NDIS_STATUS_RESOURCES);
        values.push_back(NDIS_STATUS_FAILURE);
        forkRange(state, __FUNCTION__, values);
    }else  {
        klee::ref<klee::Expr> success = state->createSymbolicValue(__FUNCTION__, klee::Expr::Int32);
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), success);
    }
}

void NdisHandlers::NdisMAllocateMapRegisters(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency == STRICT) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMAllocateMapRegistersRet)
}

void NdisHandlers::NdisMAllocateMapRegistersRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    state->jumpToSymbolicCpp();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &eax, sizeof(eax))) {
        return;
    }
    if (eax != NDIS_STATUS_SUCCESS) {
        s2e()->getDebugStream(state) <<  __FUNCTION__ << " failed" << '\n';
        return;
    }

    if (consistency == LOCAL) {
        std::vector<uint32_t> values;

        values.push_back(NDIS_STATUS_SUCCESS);
        values.push_back(NDIS_STATUS_RESOURCES);
        forkRange(state, __FUNCTION__, values);
    }else {
        klee::ref<klee::Expr> success = state->createSymbolicValue(__FUNCTION__, klee::Expr::Int32);
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), success);
    }
}

void NdisHandlers::NdisMSetAttributesEx(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    klee::ref<klee::Expr> interfaceType = readParameter(state, 4);
    s2e()->getDebugStream(state) << "InterfaceType: " << interfaceType << '\n';

    if (consistency != OVERAPPROX) {
        return;
    }

    if (((signed)m_forceAdapterType) != InterfaceTypeUndefined) {
        s2e()->getDebugStream(state) << "Forcing NIC type to " << m_forceAdapterType << '\n';
        writeParameter(state, 4, klee::ConstantExpr::create(m_forceAdapterType, klee::Expr::Int32));
    }
}


void NdisHandlers::NdisMSetAttributes(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    klee::ref<klee::Expr> interfaceType = readParameter(state, 3);
    s2e()->getDebugStream(state) << "InterfaceType: " << interfaceType << '\n';

    if (consistency != OVERAPPROX) {
        return;
    }

    if (((signed)m_forceAdapterType) != InterfaceTypeUndefined) {
        s2e()->getDebugStream(state) << "Forcing NIC type to " << m_forceAdapterType << '\n';
        writeParameter(state, 3, klee::ConstantExpr::create(m_forceAdapterType, klee::Expr::Int32));
    }

}

void NdisHandlers::NdisReadConfiguration(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    //Save parameter data that we will use on return
    //We need to put them in the state-local storage, as parameters can be mangled by the caller
    bool ok = true;

    uint32_t pStatus, pConfigParam, Handle, pConfigString;

    ok &= readConcreteParameter(state, 0, &pStatus);
    ok &= readConcreteParameter(state, 1, &pConfigParam);
    ok &= readConcreteParameter(state, 2, &Handle);
    ok &= readConcreteParameter(state, 3, &pConfigString);

    if (!ok) {
        HANDLER_TRACE_FCNFAILED();
        return;
    }

    //Display the configuration keyword
    uint64_t pc = 0;
    state->getReturnAddress(&pc);
    const ModuleDescriptor *md = m_detector->getModule(state, pc, true);
    if (md) {
        pc = md->ToNativeBase(pc);
    }

    std::string keyword;
    ok = ReadUnicodeString(state, pConfigString, keyword);
    if (ok) {
        uint32_t paramType;
        ok &= readConcreteParameter(state, 4, &paramType);

        s2e()->getMessagesStream() << "NdisReadConfiguration Module=" << (md ? md->Name : "<unknown>") <<
                " pc=" << hexval(pc) <<
                " Keyword=" << keyword <<
            " Type=" << paramType  << '\n';
    }
    ///////////////////////////////////

    if (consistency < LOCAL) {
        FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::NdisReadConfigurationRet,
                                  pStatus, pConfigParam, Handle, pConfigString);
        return;
    }

    state->undoCallAndJumpToSymbolic();


    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    klee::ref<klee::Expr> symb;
    if (consistency == OVERAPPROX) {
        symb = createFailure(state, getVariableName(state, __FUNCTION__) + "_result");
    }else {
        std::vector<uint32_t> vec;
        vec.push_back(NDIS_STATUS_RESOURCES);
        vec.push_back(NDIS_STATUS_FAILURE);
        symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
    }
    state->writeMemory(pStatus, symb);

    //Skip the call in the current state
    state->bypassFunction(5);
    incrementFailures(state);

    FUNCMON_REGISTER_RETURN_A(states[0] == state ? states[1] : states[0],
                              m_functionMonitor, NdisHandlers::NdisReadConfigurationRet,
                              pStatus, pConfigParam, Handle, pConfigString);

}

void NdisHandlers::NdisReadConfigurationRet(S2EExecutionState* state,
                                            uint32_t pStatus, uint32_t ppConfigParam, uint32_t Handle, uint32_t pConfigString)
{
    HANDLER_TRACE_RETURN();

    uint32_t Status;
    bool ok = true;
    ok &= state->readMemoryConcrete(pStatus, &Status, sizeof(Status));

    if(!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters\n";
        return;
    }

    if (!NT_SUCCESS(Status)) {
        s2e()->getDebugStream() << __FUNCTION__ << " failed with " << hexval(Status) << '\n';
        incrementFailures(state);
        return;
    }

    incrementSuccesses(state);

    uint32_t pConfigParam;

    ok &= state->readMemoryConcrete(ppConfigParam, &pConfigParam, sizeof(pConfigParam));
    if (!ok || !pConfigParam) {
        s2e()->getDebugStream() << "Could not read pointer to configuration data" << ppConfigParam << '\n';
        return;
    }

    std::string configString;
    ok = ReadUnicodeString(state, pConfigString, configString);
    if (!ok) {
        s2e()->getDebugStream() << "Could not read keyword string" << '\n';
    }

    //In all consistency models, inject symbolic value in the parameter that was read
    NDIS_CONFIGURATION_PARAMETER ConfigParam;
    ok = state->readMemoryConcrete(pConfigParam, &ConfigParam, sizeof(ConfigParam));
    if (ok) {
        if (m_ignoreKeywords.find(configString) == m_ignoreKeywords.end()) {
            //For now, we only inject integer values
            if (ConfigParam.ParameterType == NdisParameterInteger || ConfigParam.ParameterType == NdisParameterHexInteger) {
                //Write the symbolic value there.
                uint32_t valueOffset = offsetof(NDIS_CONFIGURATION_PARAMETER, ParameterData);
                std::stringstream ss;
                ss << __FUNCTION__ << "_" << configString << "_value";
                klee::ref<klee::Expr> val = state->createSymbolicValue(ss.str(), klee::Expr::Int32);
                state->writeMemory(pConfigParam + valueOffset, val);
            }
        }
    }else {
        s2e()->getDebugStream() << "Could not read configuration data" << Status << '\n';
        //Continue, this error is not too bad.
    }



    if(m_memoryChecker) {
        //Also associate the handle with every memory region
        //allocated by NdisReadConfiguration, so that they can
        //all be freed at once during NdisCloseConfiguration.

        std::string cfg = makeConfigurationRegionString(Handle, false);

        m_memoryChecker->grantMemory(state, pConfigParam, sizeof(ConfigParam),
                                     MemoryChecker::READ,
                                     cfg + "cfg");

        if(ConfigParam.ParameterType == NdisParameterString ||
                ConfigParam.ParameterType == NdisParameterMultiString) {
            m_memoryChecker->grantMemory(state,
                                         ConfigParam.ParameterData.StringData.Buffer,
                                         ConfigParam.ParameterData.StringData.Length,
                                         MemoryChecker::READ,
                                         cfg + "StringData");

        } else if(ConfigParam.ParameterType == NdisParameterBinary) {
            m_memoryChecker->grantMemory(state,
                                         ConfigParam.ParameterData.BinaryData.Buffer,
                                         ConfigParam.ParameterData.BinaryData.Length,
                                         MemoryChecker::READ,
                                         cfg + "BinaryData");
        }
    }


}

void NdisHandlers::NdisCloseConfiguration(S2EExecutionState *state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    bool ok = true;
    uint32_t Handle;

    ok &= readConcreteParameter(state, 0, &Handle);

    if (!ok) {
        HANDLER_TRACE_FCNFAILED();
        return;
    }

    if(m_memoryChecker) {
        m_memoryChecker->revokeMemory(state, makeConfigurationRegionString(Handle, true));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//BEWARE: This is a VarArg cdecl function...
void NdisHandlers::NdisWriteErrorLogEntry(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();
    std::stringstream os;

    uint32_t ErrorCode, NumberOfErrorValues;
    bool ok = true;

    ok &= readConcreteParameter(state, 1, &ErrorCode);
    ok &= readConcreteParameter(state, 2, &NumberOfErrorValues);

    if (!ok) {
        os << "Could not read error parameters" << '\n';
        return;
    }

    os << "ErrorCode=0x" << std::hex << ErrorCode << " - ";

    for (unsigned i=0; i<NumberOfErrorValues; ++i) {
        uint32_t val;
        ok &= readConcreteParameter(state, 3+i, &val);
        if (!ok) {
            os << "Could not read error parameters" << '\n';
            break;
        }
        os << val << " ";
    }

    os << '\n';

    s2e()->getDebugStream() << os.str();

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisReadPciSlotInformation(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (!m_devDesc) {
        s2e()->getWarningsStream() << __FUNCTION__ << " needs a valid symbolic device" << '\n';
        return;
    }

    uint32_t slot, offset, buffer, length;
    bool ok = true;

    ok &= readConcreteParameter(state, 1, &slot);
    ok &= readConcreteParameter(state, 2, &offset);
    ok &= readConcreteParameter(state, 3, &buffer);
    ok &= readConcreteParameter(state, 4, &length);
    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters" << '\n';
    }

    s2e()->getDebugStream(state) << "Buffer=" << hexval(buffer) <<
        " Length=" << length << " Slot=" << slot << " Offset=" << hexval(offset) << '\n';


    if (consistency != OVERAPPROX) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    uint64_t retaddr;
    ok = state->getReturnAddress(&retaddr);


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
            ss << __FUNCTION__ << "_0x" << std::hex << retaddr << "_"  << i;
            klee::ref<klee::Expr> symb = state->createSymbolicValue(ss.str(), klee::Expr::Int8);
            state->writeMemory(buffer+i, symb);
        }
    }

    //Symbolic return value
    std::stringstream ss;
    ss << __FUNCTION__ << "_" <<  std::hex << (retaddr) << "_retval";
    klee::ref<klee::Expr> symb = state->createSymbolicValue(ss.str(), klee::Expr::Int32);
    klee::ref<klee::Expr> expr = klee::UleExpr::create(symb, klee::ConstantExpr::create(length, klee::Expr::Int32));
    state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), symb);
    state->addConstraint(expr);

    state->bypassFunction(5);
    incrementFailures(state);
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisWritePciSlotInformation(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency == STRICT) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    uint32_t length;
    bool ok = true;

    ok &= readConcreteParameter(state, 4, &length);
    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters" << '\n';
    }

    uint64_t retaddr;
    ok = state->getReturnAddress(&retaddr);

    s2e()->getDebugStream(state) << " Length=" << length << '\n';

    //Symbolic return value
    std::stringstream ss;
    ss << __FUNCTION__ << "_" << std::hex << retaddr << "_retval";
    klee::ref<klee::Expr> symb = state->createSymbolicValue(ss.str(), klee::Expr::Int32);
    klee::ref<klee::Expr> expr = klee::UleExpr::create(symb, klee::ConstantExpr::create(length, klee::Expr::Int32));
    state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), symb);
    state->addConstraint(expr);

    state->bypassFunction(5);
    incrementFailures(state);
    throw CpuExitException();
}

void NdisHandlers::NdisMQueryAdapterResources(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency == STRICT) {
        return;
    }

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    bool ok = true;
    ok &= readConcreteParameter(state, 0, &plgState->pStatus);

    //XXX: Do the other parameters as well
    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters" << '\n';
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMQueryAdapterResourcesRet)
}

void NdisHandlers::NdisMQueryAdapterResourcesRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    if (!plgState->pStatus) {
        s2e()->getDebugStream() << "Status is NULL!" << '\n';
        return;
    }

    klee::ref<klee::Expr> Status = state->readMemory(plgState->pStatus, klee::Expr::Int32);
    if (!NtSuccess(s2e(), state, Status)) {
        s2e()->getDebugStream() << __FUNCTION__ << " failed with " << Status << '\n';
        incrementFailures(state);
        return;
    }

    klee::ref<klee::Expr> SymbStatus = state->createSymbolicValue(__FUNCTION__, klee::Expr::Int32);
    state->writeMemory(plgState->pStatus, SymbStatus);

    return;
}


void NdisHandlers::NdisMRegisterInterrupt(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency == OVERAPPROX) {
        //Pretend the interrupt is shared, to force the ISR to be called.
        //Make sure there is indeed a miniportisr registered
        DECLARE_PLUGINSTATE(NdisHandlersState, state);
        if (plgState->hasIsrHandler) {
            s2e()->getDebugStream() << "Pretending that the interrupt is shared." << '\n';
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
    HANDLER_TRACE_RETURN();
    state->jumpToSymbolicCpp();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    //Get the return value
    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &eax, sizeof(eax))) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": return status is not concrete" << '\n';
        return;
    }

    if (eax) {
        //The original function has failed
        s2e()->getDebugStream() << __FUNCTION__ << ": original function failed with " << hexval(eax) << '\n';
        return;
    }

    if (consistency == OVERAPPROX) {
        klee::ref<klee::Expr> success = state->createSymbolicValue(__FUNCTION__, klee::Expr::Int32);
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), success);
    }else

    //ExecutionConsistencyModel: LOCAL
    if (consistency == LOCAL) {
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
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency == STRICT) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMRegisterIoPortRangeRet)
}

void NdisHandlers::NdisMRegisterIoPortRangeRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    state->jumpToSymbolicCpp();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    //Get the return value
    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &eax, sizeof(eax))) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": return status is not concrete" << '\n';
        return;
    }

    if (eax) {
        //The original function has failed
        return;
    }

    if (consistency == OVERAPPROX) {
        klee::ref<klee::Expr> success = state->createSymbolicValue(__FUNCTION__, klee::Expr::Int32);
        state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), success);
    }else

    //ExecutionConsistencyModel: LOCAL
    if (consistency == LOCAL) {
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
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    //Save parameter data that we will use on return
    uint32_t pStatus, pNetworkAddress, pNetworkAddressLength, ConfigurationHandle;
    bool ok = true;

    ok &= readConcreteParameter(state, 0, &pStatus);
    ok &= readConcreteParameter(state, 1, &pNetworkAddress);
    ok &= readConcreteParameter(state, 2, &pNetworkAddressLength);
    ok &= readConcreteParameter(state, 3, &ConfigurationHandle);

    if (!ok) {
        s2e()->getDebugStream() << __FUNCTION__ << " could not read stack parameters (maybe symbolic?) "  << '\n';
        return;
    }

    if (consistency < LOCAL) {
        FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::NdisReadNetworkAddressRet, pStatus,
                                  pNetworkAddress, pNetworkAddressLength, ConfigurationHandle);
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    klee::ref<klee::Expr> symb;
    if (consistency == OVERAPPROX) {
        symb = createFailure(state, getVariableName(state, __FUNCTION__) + "_result");
    }else {
        std::vector<uint32_t> vec;
        vec.push_back(NDIS_STATUS_FAILURE);
        symb = addDisjunctionToConstraints(state, getVariableName(state, __FUNCTION__) + "_result", vec);
    }
    state->writeMemory(pStatus, symb);

    //Skip the call in the current state
    state->bypassFunction(4);
    incrementFailures(state);

    FUNCMON_REGISTER_RETURN_A(states[0] == state ? states[1] : states[0],
                              m_functionMonitor, NdisHandlers::NdisReadNetworkAddressRet, pStatus,
                              pNetworkAddress, pNetworkAddressLength, ConfigurationHandle);
}

void NdisHandlers::NdisReadNetworkAddressRet(S2EExecutionState* state, uint32_t pStatus, uint32_t pNetworkAddress,
                                             uint32_t pNetworkAddressLength, uint32_t ConfigurationHandle)
{
    HANDLER_TRACE_RETURN();

    bool ok = true;
    NDIS_STATUS Status;

    ok &= state->readMemoryConcrete(pStatus, &Status, sizeof(Status));
    if(!ok) {
        s2e()->getDebugStream() << "Can not read result\n";
        return;
    }

    if (!NT_SUCCESS(Status)) {
        s2e()->getDebugStream() << __FUNCTION__ << " failed with " << hexval(Status) << 'n';
        incrementFailures(state);
        return;
    }

    state->jumpToSymbolicCpp();

    incrementSuccesses(state);

    uint32_t Length, NetworkAddress;

    ok &= state->readMemoryConcrete(pNetworkAddressLength, &Length, sizeof(Length));
    ok &= state->readMemoryConcrete(pNetworkAddress, &NetworkAddress, sizeof(NetworkAddress));
    if (!ok || !NetworkAddress) {
        s2e()->getDebugStream() << "Could not read network address pointer and/or its length" << Status << '\n';
        return;
    }

    if (m_memoryChecker) {
        std::string cfg = makeConfigurationRegionString(ConfigurationHandle, false);
        m_memoryChecker->grantMemory(state, NetworkAddress, Length,
                                     MemoryChecker::READ, cfg + "NetworkAddress");
    }

    //In all cases, inject symbolic values in the returned buffer
    for (unsigned i=0; i<Length; ++i) {

        if (m_networkAddress.size() > 0) {
            s2e()->getDebugStream() << "NetworkAddress[" << i << "]=" <<
              hexval(m_networkAddress[i % m_networkAddress.size()]) << '\n';

            state->writeMemoryConcrete(NetworkAddress + i, &m_networkAddress[i % m_networkAddress.size()], 1);
        }else {
            std::stringstream ss;
            ss << __FUNCTION__ << "_" << i;
            klee::ref<klee::Expr> val = state->createSymbolicValue(ss.str(), klee::Expr::Int8);
            state->writeMemory(NetworkAddress + i, val);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

void NdisHandlers::NdisMRegisterMiniport(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency != STRICT) {
        FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMRegisterMiniportRet)
    }

    //Extract the function pointers from the passed data structure
    uint32_t pMiniport;
    if (!state->readMemoryConcrete(state->getSp() + sizeof(pMiniport) * (1+1), &pMiniport, sizeof(pMiniport))) {
        s2e()->getMessagesStream() << "Could not read pMiniport address from the stack" << '\n';
        return;
    }

    s2e()->getMessagesStream() << "NDIS_MINIPORT_CHARACTERISTICS @" << hexval(pMiniport) << '\n';

    s2e::windows::NDIS_MINIPORT_CHARACTERISTICS32 Miniport;
    if (!state->readMemoryConcrete(pMiniport, &Miniport, sizeof(Miniport))) {
        s2e()->getMessagesStream() << "Could not read NDIS_MINIPORT_CHARACTERISTICS" << '\n';
        return;
    }

    //Register each handler
    NDIS_REGISTER_ENTRY_POINT(Miniport.CheckForHangHandler, CheckForHang);
    NDIS_REGISTER_ENTRY_POINT(Miniport.InitializeHandler, InitializeHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.DisableInterruptHandler, DisableInterruptHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.EnableInterruptHandler, EnableInterruptHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.HaltHandler, HaltHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.HandleInterruptHandler, HandleInterruptHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.ISRHandler, ISRHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.QueryInformationHandler, QueryInformationHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.ReconfigureHandler, ReconfigureHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.ResetHandler, ResetHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.SendHandler, SendHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.SendPacketsHandler, SendPacketsHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.SetInformationHandler, SetInformationHandler);
    NDIS_REGISTER_ENTRY_POINT(Miniport.TransferDataHandler, TransferDataHandler);

    if (Miniport.ISRHandler) {
        DECLARE_PLUGINSTATE(NdisHandlersState, state);
        plgState->hasIsrHandler = true;
    }
}

void NdisHandlers::NdisMRegisterMiniportRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    state->jumpToSymbolicCpp();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    //Get the return value
    uint32_t eax;
    if (!state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &eax, sizeof(eax))) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": return status is not concrete" << '\n';
        return;
    }

    if (consistency == OVERAPPROX) {
        //Replace the return value with a symbolic value
        if ((int)eax>=0) {
            klee::ref<klee::Expr> ret = state->createSymbolicValue(__FUNCTION__, klee::Expr::Int32);
            state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]), ret);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
//These are driver entry points
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::NdisMStatusHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::NdisMStatusHandlerRet)
    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    uint32_t status;
    if (!readConcreteParameter(state, 1, &status)) {
        s2e()->getDebugStream() << "Could not get cable status in " << __FUNCTION__ << '\n';
        return;
    }

    s2e()->getDebugStream() << "Status is " << hexval(status) << '\n';

    if (status == NDIS_STATUS_MEDIA_CONNECT) {
        s2e()->getDebugStream() << "Cable is connected" << '\n';
        plgState->cableStatus = NdisHandlersState::CONNECTED;
    }else if (status == NDIS_STATUS_MEDIA_DISCONNECT) {
        s2e()->getDebugStream() << "Cable is disconnected" << '\n';
        plgState->cableStatus = NdisHandlersState::DISCONNECTED;
    }
}

void NdisHandlers::NdisMStatusHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::CheckForHang(S2EExecutionState* state, FunctionMonitorState *fns)
{
    static bool exercised = false;

    HANDLER_TRACE_CALL();

    if (exercised) {
        uint32_t success = 1;
        state->writeCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &success, sizeof(success));
        state->bypassFunction(1);
        throw CpuExitException();
    }

    exercised = true;
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::CheckForHangRet)
}

void NdisHandlers::CheckForHangRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    if (consistency == OVERAPPROX) {
        //Pretend we did not hang
        //uint32_t success = 0;
        //state->writeCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &success, sizeof(success));
    }

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::InitializeHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();

    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::InitializeHandlerRet)
    /* Make the medium array symbolic */
    uint32_t pMediumArray, MediumArraySize;

    if (!readConcreteParameter(state, 2, &pMediumArray)) {
        s2e()->getDebugStream(state) << "Could not read pMediumArray" << '\n';
        return;
    }

    if (!readConcreteParameter(state, 3, &MediumArraySize)) {
        s2e()->getDebugStream(state) << "Could not read MediumArraySize" << '\n';
        return;
    }

    //Register API exported in the handle
    uint32_t NdisHandle;
    if (!readConcreteParameter(state, 4, &NdisHandle)) {
        s2e()->getDebugStream(state) << "Could not read NdisHandle\n";
        return;
    }

    grantMiniportAdapterContext(state, 4);

    uint32_t pStatusHandler, pSendCompleteHandler;
    if (!state->readMemoryConcrete(NdisHandle + NDIS_M_STATUS_HANDLER_OFFSET, &pStatusHandler, sizeof(pStatusHandler))) {
        s2e()->getMessagesStream() << "Could not read pointer to status handler\n";
        return;
    }

    if (!state->readMemoryConcrete(NdisHandle + NDIS_M_SEND_COMPLETE_HANDLER_OFFSET, &pSendCompleteHandler, sizeof(pSendCompleteHandler))) {
        s2e()->getMessagesStream() << "Could not read pointer to send complete handler\n";
        return;
    }

    s2e()->getDebugStream() << "NDIS_M_STATUS_HANDLER " << hexval(pStatusHandler) << '\n';

    if(m_memoryChecker) {
        //const ModuleDescriptor* module = m_detector->getModule(state, state->getPc());
        m_memoryChecker->grantMemory(state, pMediumArray, MediumArraySize*4,
                             MemoryChecker::READ, "ndis:args:MiniportInitialize:MediumArray");
    }

    NDIS_REGISTER_ENTRY_POINT(pStatusHandler, NdisMStatusHandler);
    NDIS_REGISTER_ENTRY_POINT(pSendCompleteHandler, NdisMSendCompleteHandler);



    if (consistency == STRICT) {
        return;
    }

    //if (consistency == LOCAL)
    {
        //Make size properly constrained
        if (pMediumArray) {
            for (unsigned i=0; i<MediumArraySize; i++) {
                std::stringstream ss;
                ss << "MediumArray" << std::dec << "_" << i;
                state->writeMemory(pMediumArray + i * 4, state->createSymbolicValue(ss.str(), klee::Expr::Int32));
            }

            klee::ref<klee::Expr> SymbSize = state->createSymbolicValue("MediumArraySize", klee::Expr::Int32);

            klee::ref<klee::Expr> Constr = klee::UleExpr::create(SymbSize,
                                                               klee::ConstantExpr::create(MediumArraySize, klee::Expr::Int32));
            state->addConstraint(Constr);
            writeParameter(state, 3, SymbSize);
        }
    }

}

void NdisHandlers::InitializeHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    const ModuleDescriptor* module = m_detector->getModule(state, state->getPc());

    if(m_memoryChecker) {
        m_memoryChecker->revokeMemory(state, "ndis:args:MiniportInitialize:*");
        //revokeMiniportAdapterContext(state);
    }

    //Check the success status, kill if failure
    klee::ref<klee::Expr> eax = state->readCpuRegister(offsetof(CPUX86State, regs[R_EAX]), klee::Expr::Int32);
    klee::Solver *solver = s2e()->getExecutor()->getSolver();
    bool isTrue;
    klee::ref<klee::Expr> eq = klee::EqExpr::create(eax, klee::ConstantExpr::create(0, eax.get()->getWidth()));
    if (!solver->mayBeTrue(klee::Query(state->constraints, eq), isTrue)) {
        s2e()->getMessagesStream(state) << "Killing state "  << state->getID() <<
                " because InitializeHandler failed to determine success" << '\n';
        s2e()->getExecutor()->terminateStateEarly(*state, "InitializeHandler solver failed");
    }

    if (!isTrue) {
        s2e()->getMessagesStream(state) << "Killing state "  << state->getID() <<
                                           " because InitializeHandler failed with " << eax << '\n';
        detectLeaks(state, *module);
        s2e()->getExecutor()->terminateStateEarly(*state, "InitializeHandler failed");
        return;
    }

    //Make sure we succeed by adding a constraint on the eax value
    klee::ref<klee::Expr> constr = klee::SgeExpr::create(eax, klee::ConstantExpr::create(0, eax.get()->getWidth()));
    state->addConstraint(constr);

    DECLARE_PLUGINSTATE(NdisHandlersState, state);
    plgState->exercisingInitEntryPoint = false;

    s2e()->getDebugStream(state) << "InitializeHandler succeeded with " << eax << '\n';

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::DisableInterruptHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    if (!m_devDesc) {
        s2e()->getWarningsStream() << __FUNCTION__ << " needs a valid symbolic device" << '\n';
        return;
    }
    //XXX: broken
    //grantMiniportAdapterContext(state, 0);

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::DisableInterruptHandlerRet)
    m_devDesc->setInterrupt(false);
}

void NdisHandlers::DisableInterruptHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    //revokeMiniportAdapterContext(state);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::EnableInterruptHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
//    grantMiniportAdapterContext(state, 0);
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::EnableInterruptHandlerRet)
}

void NdisHandlers::EnableInterruptHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
//    revokeMiniportAdapterContext(state);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::HaltHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    //grantMiniportAdapterContext(state, 0);

    if (consistency != OVERAPPROX) {
        FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::HaltHandlerRet)
        return;
    }

    DECLARE_PLUGINSTATE(NdisHandlersState, state);
    if (!plgState->shutdownHandler) {
        FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::HaltHandlerRet);
        return;
    }

    state->undoCallAndJumpToSymbolic();

    bool oldForkStatus = state->isForkingEnabled();
    state->enableForking();

    //Fork the states. In one of them run the shutdown handler
    klee::ref<klee::Expr> isFake = state->createSymbolicValue("FakeShutdown", klee::Expr::Int8);
    klee::ref<klee::Expr> cond = klee::EqExpr::create(isFake, klee::ConstantExpr::create(1, klee::Expr::Int8));

    klee::Executor::StatePair sp = s2e()->getExecutor()->fork(*state, cond, false);

    S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
    S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

    //One of the states will run the shutdown handler
    ts->writeCpuState(offsetof(CPUX86State, eip), plgState->shutdownHandler, sizeof(uint32_t)*8);

    FUNCMON_REGISTER_RETURN(ts, fns, NdisHandlers::HaltHandlerRet)
    FUNCMON_REGISTER_RETURN(fs, fns, NdisHandlers::HaltHandlerRet)

    ts->setForking(oldForkStatus);
    fs->setForking(oldForkStatus);
}

void NdisHandlers::HaltHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();

    if(m_memoryChecker) {
        const ModuleDescriptor* module = m_detector->getModule(state, state->getPc());
        detectLeaks(state, *module);
    }

    //There is nothing more to execute, kill the state
    s2e()->getExecutor()->terminateStateEarly(*state, "NdisHalt");

}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::HandleInterruptHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    //grantMiniportAdapterContext(state, 0);

    DECLARE_PLUGINSTATE(NdisHandlersState, state);
    if (plgState->exercisingInitEntryPoint) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::HandleInterruptHandlerRet)
    plgState->isrHandlerExecuted = true;
    plgState->isrHandlerQueued = false;
}

void NdisHandlers::HandleInterruptHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    //revokeMiniportAdapterContext(state);

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ISRHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    //grantMiniportAdapterContext(state, 2);

    if (!m_devDesc) {
        s2e()->getWarningsStream() << __FUNCTION__ << " needs a valid symbolic device" << '\n';
        return;
    }


    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::ISRHandlerRet)

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    bool ok = true;
    ok &= readConcreteParameter(state, 0, &plgState->isrRecognized);
    ok &= readConcreteParameter(state, 1, &plgState->isrQueue);

    if (!ok) {
        s2e()->getDebugStream(state) << "Error reading isrRecognized and isrQueue"<< '\n';
    }
    m_devDesc->setInterrupt(false);
}

void NdisHandlers::ISRHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();

    //revokeMiniportAdapterContext(state);

    if (!m_devDesc) {
        s2e()->getWarningsStream() << __FUNCTION__ << " needs a valid symbolic device" << '\n';
        return;
    }


    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    if (plgState->exercisingInitEntryPoint) {
        m_devDesc->setInterrupt(false);
        return;
    }

    uint8_t isrRecognized=0, isrQueue=0;
    bool ok = true;
    ok &= state->readMemoryConcrete(plgState->isrRecognized, &isrRecognized, sizeof(isrRecognized));
    ok &= state->readMemoryConcrete(plgState->isrQueue, &isrQueue, sizeof(isrQueue));

    s2e()->getDebugStream(state) << "ISRRecognized=" << (bool)isrRecognized <<
            " isrQueue="<< (bool)isrQueue << '\n';

    if (!ok) {
        s2e()->getExecutor()->terminateStateEarly(*state, "Could not determine whether the NDIS driver queued the interrupt");
    }else {
        //Kill the state if no interrupt will ever occur in it.
        if ((!isrRecognized || !isrQueue) && (!plgState->isrHandlerExecuted && !plgState->isrHandlerQueued)) {
            s2e()->getExecutor()->terminateStateEarly(*state, "The state will not trigger any interrupt");
        }

        if (isrRecognized && isrQueue) {
            plgState->isrHandlerQueued = true;
        }
    }

    m_devDesc->setInterrupt(false);

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::QuerySetInformationHandler(S2EExecutionState* state, FunctionMonitorState *fns, bool isQuery)
{
    DECLARE_PLUGINSTATE(NdisHandlersState, state);
    ExecutionConsistencyModel consistency = getConsistency(state, __FUNCTION__);

    s2e()->getDebugStream() << "Called with OID=" << hexval(plgState->oid) << '\n';

    if (consistency != OVERAPPROX) {
        return;
    }

    std::stringstream ss;
    ss << __FUNCTION__ << "_OID";
    klee::ref<klee::Expr> symbOid = state->createSymbolicValue(ss.str(), klee::Expr::Int32);

    klee::ref<klee::Expr> isFakeOid = state->createSymbolicValue("IsFakeOid", klee::Expr::Int8);
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
    klee::ref<klee::Expr> symbBufferSize = state->createSymbolicValue("QuerySetInfoBufferSize", klee::Expr::Int32);
    current_sp -= newBufferSize;
    uint32_t newBufferPtr = current_sp;

    //XXX: Do OID-aware injection
    for (unsigned i=0; i<newBufferSize; ++i) {
        std::stringstream ssb;
        ssb << __FUNCTION__ << "_buffer_" << i;
        klee::ref<klee::Expr> symbByte = state->createSymbolicValue(ssb.str(), klee::Expr::Int8);
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

    bool oldForkStatus = state->isForkingEnabled();
    state->enableForking();

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
    ts->writeCpuRegisterConcrete(offsetof(CPUX86State, regs[R_ESP]), &current_sp, sizeof(current_sp));

    //Add a constraint for the buffer size
    klee::ref<klee::Expr> symbBufferConstr = klee::UleExpr::create(symbBufferSize, klee::ConstantExpr::create(newBufferSize, klee::Expr::Int32));
    ts->addConstraint(symbBufferConstr);

    //Register separately the return handler,
    //since the stack pointer is different in the two states
    FUNCMON_REGISTER_RETURN(ts, fns, NdisHandlers::QueryInformationHandlerRet)
    FUNCMON_REGISTER_RETURN(fs, fns, NdisHandlers::QueryInformationHandlerRet)

    ts->setForking(oldForkStatus);
    fs->setForking(oldForkStatus);
}

void NdisHandlers::QuerySetInformationHandlerRet(S2EExecutionState* state, bool isQuery)
{
    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    if (plgState->fakeoid) {
        //Stop inconsistent execution immediately
        s2e()->getExecutor()->terminateStateEarly(*state, "Killing state with fake OID");
    }

    s2e()->getDebugStream(state) << "State is not fake, continuing..." << '\n';

    if (isQuery) {
        //Keep only those states that have a connected cable
        uint32_t status;
        if (state->readMemoryConcrete(plgState->pInformationBuffer, &status, sizeof(status))) {
            s2e()->getDebugStream() << "Status=" << hexval(status) << '\n';
            if (plgState->oid == OID_GEN_MEDIA_CONNECT_STATUS) {
                s2e()->getDebugStream(state) << "OID_GEN_MEDIA_CONNECT_STATUS is " << status << '\n';
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
    //XXX: what about multiple driver?
    static bool alreadyExplored = false;

    HANDLER_TRACE_CALL();

    state->undoCallAndJumpToSymbolic();

    //grantMiniportAdapterContext(state, 0);

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    plgState->oid = (uint32_t)-1;
    plgState->pInformationBuffer = 0;
    uint32_t Buffer, BufferLength, BytesNeeded, BytesWritten;

    readConcreteParameter(state, 1, &plgState->oid);
    readConcreteParameter(state, 2, &plgState->pInformationBuffer);
    readConcreteParameter(state, 2, &Buffer);
    readConcreteParameter(state, 3, &BufferLength);
    readConcreteParameter(state, 4, &BytesWritten);
    readConcreteParameter(state, 5, &BytesNeeded);


    if (m_memoryChecker) {
        if (Buffer && BufferLength && !m_windowsMonitor->isOnTheStack(state, Buffer)) {
            m_memoryChecker->grantMemory(state, Buffer, BufferLength, MemoryChecker::READWRITE,
                  "ndis:MiniportQueryInformation:Buffer");
        }

        if (!m_windowsMonitor->isOnTheStack(state, BytesWritten)) {
            m_memoryChecker->grantMemory(state, BytesWritten, sizeof(uint32_t), MemoryChecker::READWRITE,
                  "ndis:MiniportQueryInformation:BytesWritten");
        }

        if (!m_windowsMonitor->isOnTheStack(state, BytesNeeded)) {
            m_memoryChecker->grantMemory(state, BytesNeeded, sizeof(uint32_t), MemoryChecker::READWRITE,
                  "ndis:MiniportQueryInformation:BytesNeeded");
        }
    }

    s2e()->getDebugStream(state) << "OID=" << hexval(plgState->oid) << '\n';

    if (alreadyExplored) {
        FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::QueryInformationHandlerRet)
        s2e()->getDebugStream(state) << "Already explored " << __FUNCTION__ << " at " << hexval(state->getPc()) << '\n';
        return;
    }


    alreadyExplored = true;
    QuerySetInformationHandler(state, fns, true);

}

void NdisHandlers::QueryInformationHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    state->jumpToSymbolicCpp();

    //revokeMiniportAdapterContext(state);
    if (m_memoryChecker) {
        m_memoryChecker->revokeMemory(state, "ndis:MiniportQueryInformation*");
    }

    QuerySetInformationHandlerRet(state, true);

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::SetInformationHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    static bool alreadyExplored = false;

    HANDLER_TRACE_CALL();
    //grantMiniportAdapterContext(state, 0);

    state->undoCallAndJumpToSymbolic();

    DECLARE_PLUGINSTATE(NdisHandlersState, state);

    plgState->oid = (uint32_t)-1;
    plgState->pInformationBuffer = 0;

    uint32_t Buffer, BufferLength, BytesRead, BytesNeeded;
    readConcreteParameter(state, 1, &plgState->oid);
    readConcreteParameter(state, 2, &plgState->pInformationBuffer);
    readConcreteParameter(state, 2, &Buffer);
    readConcreteParameter(state, 3, &BufferLength);
    readConcreteParameter(state, 4, &BytesRead);
    readConcreteParameter(state, 5, &BytesNeeded);

    if (m_memoryChecker) {
        if (Buffer && BufferLength && !m_windowsMonitor->isOnTheStack(state, Buffer)) {
            m_memoryChecker->grantMemory(state, Buffer, BufferLength, MemoryChecker::READWRITE,
                  "ndis:MiniportSetInformation:Buffer");
        }

        if (!m_windowsMonitor->isOnTheStack(state, BytesRead)) {
            m_memoryChecker->grantMemory(state, BytesRead, sizeof(uint32_t), MemoryChecker::READWRITE,
                  "ndis:MiniportSetInformation:BytesRead");
        }

        if (!m_windowsMonitor->isOnTheStack(state, BytesNeeded)) {
            m_memoryChecker->grantMemory(state, BytesNeeded, sizeof(uint32_t), MemoryChecker::READWRITE,
                  "ndis:MiniportSetInformation:BytesNeeded");
        }

    }

    s2e()->getDebugStream(state) << "OID=" << hexval(plgState->oid) << '\n';

    if (alreadyExplored) {
        FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::SetInformationHandlerRet)
        s2e()->getDebugStream(state) << "Already explored " << __FUNCTION__ << " at " << hexval(state->getPc()) << '\n';
        return;
    }


    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::SetInformationHandlerRet)

    alreadyExplored = true;
    QuerySetInformationHandler(state, fns, false);
}

void NdisHandlers::SetInformationHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    //revokeMiniportAdapterContext(state);

    if (m_memoryChecker) {
        m_memoryChecker->revokeMemory(state, "ndis:MiniportSetInformation*");
    }

    QuerySetInformationHandlerRet(state, false);

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ReconfigureHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    //grantMiniportAdapterContext(state, 1);

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::ReconfigureHandlerRet)
}

void NdisHandlers::ReconfigureHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    //revokeMiniportAdapterContext(state);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::ResetHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    //grantMiniportAdapterContext(state, 1);
    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::ResetHandlerRet)
}

void NdisHandlers::ResetHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    //revokeMiniportAdapterContext(state);

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::SendHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    //grantMiniportAdapterContext(state, 0);

    uint32_t pPacket;
    readConcreteParameter(state, 1, &pPacket);
    grantPacket(state, pPacket, 0);

    FUNCMON_REGISTER_RETURN_A(state, fns, NdisHandlers::SendHandlerRet, pPacket)
}

void NdisHandlers::SendHandlerRet(S2EExecutionState* state, uint32_t pPacket)
{
    HANDLER_TRACE_RETURN();
    //revokeMiniportAdapterContext(state);

    uint32_t status;
    bool ok;
    if (!(ok = state->readCpuRegisterConcrete(offsetof(CPUX86State, regs[R_EAX]), &status, sizeof(status)))) {
        s2e()->getWarningsStream() << __FUNCTION__  << ": return status is not concrete\n";
    }

    if (NT_SUCCESS(status)) {
        incrementSuccesses(state);
    } else {
        incrementFailures(state);
    }

    if (ok && m_memoryChecker) {
        s2e()->getDebugStream() << "SendHandler status=" << hexval(status) << '\n';
        switch (status) {
            //The driver (or its NIC) has accepted the packet data for transmission, so MiniportSend is returning the packet,
            //which NDIS will return to the protocol.
            case NDIS_STATUS_SUCCESS:

            //NDIS returns the given packet back to the protocol with an error status.
            case NDIS_STATUS_RESOURCES:

            //The given packet was invalid or unacceptable to the NIC
            case NDIS_STATUS_FAILURE:

            default:
                revokePacket(state, pPacket);
                break;

            //The driver will complete the packet asynchronously with a call to NdisMSendComplete.
            case NDIS_STATUS_PENDING:
                //Don't revoke right here.
                break;


        }
    }

    if (m_devDesc) {
        m_devDesc->setInterrupt(true);
    }

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//Entry point internal to NDIS
void NdisHandlers::NdisMSendCompleteHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    uint32_t pPacket;
    readConcreteParameter(state, 1, &pPacket);
    if (m_memoryChecker) {
        revokePacket(state, pPacket);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::SendPacketsHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    //grantMiniportAdapterContext(state, 0);

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::SendPacketsHandlerRet)
}

void NdisHandlers::SendPacketsHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    //revokeMiniportAdapterContext(state);

    m_devDesc->setInterrupt(true);

    if (m_manager) {
        m_manager->succeedState(state);
        m_functionMonitor->eraseSp(state, state->getPc());
        throw CpuExitException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NdisHandlers::TransferDataHandler(S2EExecutionState* state, FunctionMonitorState *fns)
{
    HANDLER_TRACE_CALL();
    //grantMiniportAdapterContext(state, 2);

    FUNCMON_REGISTER_RETURN(state, fns, NdisHandlers::TransferDataHandlerRet)
}

void NdisHandlers::TransferDataHandlerRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();
    //revokeMiniportAdapterContext(state);
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
    isrHandlerQueued = false;
    faketimer = false;
    shutdownHandler = 0;   
    cableStatus = UNKNOWN;
    ProtocolReservedLength = 0;
    exercisingInitEntryPoint = true;
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
