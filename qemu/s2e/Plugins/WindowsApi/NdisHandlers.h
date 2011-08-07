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

#ifndef S2E_PLUGINS_NDISHANDLERS_H
#define S2E_PLUGINS_NDISHANDLERS_H

#include <s2e/Plugin.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsCrashDumpGenerator.h>
#include <s2e/Plugins/SymbolicHardware.h>


#include <s2e/Plugins/StateManager.h>

#define CURRENT_CLASS NdisHandlers
#include "Api.h"
#include "Ndis.h"

namespace s2e {
namespace plugins {

class MemoryChecker;

class NdisHandlers : public WindowsApi
{
    S2E_PLUGIN
public:
    NdisHandlers(S2E* s2e): WindowsApi(s2e) {}

    void initialize();

private:
    StateManager *m_manager;
    SymbolicHardware *m_hw;
    WindowsCrashDumpGenerator *m_crashdumper;

    MemoryChecker *m_memoryChecker;

    //Modules we want to intercept
    StringSet m_modules;
    StringSet m_loadedModules;
    std::string m_hwId;
    DeviceDescriptor *m_devDesc;

    StringSet m_ignoreKeywords;
    unsigned m_timerIntervalFactor;
    uint32_t m_forceAdapterType;
    bool m_generateDumpOnLoad;

    std::vector<uint8_t> m_networkAddress;

    //Pair of address + private pointer for all registered timer entry points
    typedef std::set<std::pair<uint32_t, uint32_t> > TimerEntryPoints;
    TimerEntryPoints m_timerEntryPoints;

    void onModuleLoad(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void onModuleUnload(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        );

    const ModuleDescriptor *calledFromModule(S2EExecutionState *s);

    DECLARE_ENTRY_POINT(entryPoint);

    DECLARE_ENTRY_POINT(NdisMRegisterMiniport);
    DECLARE_ENTRY_POINT(NdisAllocateMemory, uint32_t Address, uint32_t Length);
    DECLARE_ENTRY_POINT(NdisAllocateMemoryWithTag, uint32_t Address, uint32_t Length);
    DECLARE_ENTRY_POINT(NdisAllocateMemoryWithTagPriority, uint32_t Length);
    DECLARE_ENTRY_POINT_CO(NdisFreeMemory);
    DECLARE_ENTRY_POINT(NdisMAllocateSharedMemory);
    DECLARE_ENTRY_POINT(NdisMFreeSharedMemory);
    DECLARE_ENTRY_POINT(NdisMRegisterIoPortRange);
    DECLARE_ENTRY_POINT(NdisMMapIoSpace);
    DECLARE_ENTRY_POINT(NdisMRegisterInterrupt);
    DECLARE_ENTRY_POINT(NdisMQueryAdapterInstanceName);
    DECLARE_ENTRY_POINT(NdisMQueryAdapterResources);
    DECLARE_ENTRY_POINT(NdisMAllocateMapRegisters);
    DECLARE_ENTRY_POINT(NdisMInitializeTimer);
    DECLARE_ENTRY_POINT(NdisMSetAttributesEx);
    DECLARE_ENTRY_POINT(NdisMSetAttributes);
    DECLARE_ENTRY_POINT(NdisSetTimer);
    DECLARE_ENTRY_POINT(NdisMRegisterAdapterShutdownHandler);
    DECLARE_ENTRY_POINT(NdisReadNetworkAddress);
    DECLARE_ENTRY_POINT(NdisReadConfiguration);
    DECLARE_ENTRY_POINT_CO(NdisCloseConfiguration);
    DECLARE_ENTRY_POINT(NdisWriteErrorLogEntry);

    DECLARE_ENTRY_POINT(NdisAllocatePacket, uint32_t pStatus, uint32_t pPacket);
    DECLARE_ENTRY_POINT_CO(NdisFreePacket);

    DECLARE_ENTRY_POINT(NdisAllocateBuffer, uint32_t pStatus, uint32_t pBuffer, uint32_t Length);
    DECLARE_ENTRY_POINT_CO(IoFreeMdl);

    DECLARE_ENTRY_POINT(NdisReadPciSlotInformation);
    DECLARE_ENTRY_POINT(NdisWritePciSlotInformation);
    
    //These are contained inside the miniport handle
    DECLARE_ENTRY_POINT(NdisMStatusHandler);

    //XXX: Move this to ntoskrnl module
    DECLARE_ENTRY_POINT(RtlEqualUnicodeString);
    DECLARE_ENTRY_POINT(GetSystemUpTime);
    DECLARE_ENTRY_POINT(KeStallExecutionProcessor);

    //This is an internal function in ntoskrnl.exe
    DECLARE_ENTRY_POINT(DebugPrint);

    DECLARE_ENTRY_POINT(CheckForHang);
    DECLARE_ENTRY_POINT(InitializeHandler);
    DECLARE_ENTRY_POINT(DisableInterruptHandler);
    DECLARE_ENTRY_POINT(EnableInterruptHandler);
    DECLARE_ENTRY_POINT(HaltHandler);
    DECLARE_ENTRY_POINT(HandleInterruptHandler);
    DECLARE_ENTRY_POINT(ISRHandler);
    DECLARE_ENTRY_POINT(QueryInformationHandler);
    DECLARE_ENTRY_POINT(ReconfigureHandler);
    DECLARE_ENTRY_POINT(ResetHandler);
    DECLARE_ENTRY_POINT(SendHandler);
    DECLARE_ENTRY_POINT(SendPacketsHandler);
    DECLARE_ENTRY_POINT(SetInformationHandler);
    DECLARE_ENTRY_POINT(TransferDataHandler);

    DECLARE_ENTRY_POINT(NdisTimerEntryPoint);
    DECLARE_ENTRY_POINT(NdisShutdownEntryPoint);

    void QuerySetInformationHandler(S2EExecutionState* state, FunctionMonitorState *fns, bool isQuery);
    void QuerySetInformationHandlerRet(S2EExecutionState* state, bool isQuery);
};

//XXX: We assume that we are testing only one driver at a time.
//All the fields must also be per-driver for correctness in case multiple
//NDIS drivers are tested.
class NdisHandlersState: public PluginState
{
public:
    enum CableStatus {
      UNKNOWN, CONNECTED, DISCONNECTED
    };

private:
    uint32_t pStatus, pNetworkAddress, pNetworkAddressLength;
    uint32_t pConfigParam, pConfigString;
    bool hasIsrHandler;
    uint32_t oid, pInformationBuffer;
    bool fakeoid, faketimer;
    uint32_t val1, val2, val3, val4;

    uint32_t isrRecognized, isrQueue;
    bool isrHandlerExecuted;
    bool isrHandlerQueued;

    uint32_t shutdownHandler;

    //Indicates whether the cable is plugged or not in the current state.
    //It may be unknown
    CableStatus cableStatus;    
public:
    NdisHandlersState();
    virtual ~NdisHandlersState();
    virtual NdisHandlersState* clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    friend class NdisHandlers;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
