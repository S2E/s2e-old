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

    //Modules we want to intercept
    StringSet m_modules;
    StringSet m_loadedModules;
    std::string m_hwId;
    DeviceDescriptor *m_devDesc;

    StringSet m_ignoreKeywords;
    unsigned m_timerIntervalFactor;
    uint32_t m_forceAdapterType;
    bool m_generateDumpOnLoad;

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

    bool calledFromModule(S2EExecutionState *s);

    DECLARE_ENTRY_POINT(entryPoint);

    DECLARE_ENTRY_POINT(NdisMRegisterMiniport);
    DECLARE_ENTRY_POINT(NdisAllocateMemory);
    DECLARE_ENTRY_POINT(NdisAllocateMemoryWithTag);
    DECLARE_ENTRY_POINT(NdisMAllocateSharedMemory);
    DECLARE_ENTRY_POINT(NdisMFreeSharedMemory);
    DECLARE_ENTRY_POINT(NdisMRegisterIoPortRange);
    DECLARE_ENTRY_POINT(NdisMMapIoSpace);
    DECLARE_ENTRY_POINT(NdisMRegisterInterrupt);
    DECLARE_ENTRY_POINT(NdisMQueryAdapterResources);
    DECLARE_ENTRY_POINT(NdisMAllocateMapRegisters);
    DECLARE_ENTRY_POINT(NdisMInitializeTimer);
    DECLARE_ENTRY_POINT(NdisMSetAttributesEx);
    DECLARE_ENTRY_POINT(NdisMSetAttributes);
    DECLARE_ENTRY_POINT(NdisSetTimer);
    DECLARE_ENTRY_POINT(NdisMRegisterAdapterShutdownHandler);
    DECLARE_ENTRY_POINT(NdisReadNetworkAddress);
    DECLARE_ENTRY_POINT(NdisReadConfiguration);
    DECLARE_ENTRY_POINT(NdisWriteErrorLogEntry);

    DECLARE_ENTRY_POINT(NdisReadPciSlotInformation);
    DECLARE_ENTRY_POINT(NdisWritePciSlotInformation);
    
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
private:
    uint32_t pStatus, pNetworkAddress, pNetworkAddressLength;
    uint32_t pConfigParam, pConfigString;
    bool hasIsrHandler;
    uint32_t oid, pInformationBuffer;
    bool fakeoid, faketimer;
    uint32_t val1, val2, val3;

    uint32_t isrRecognized, isrQueue;
    bool isrHandlerExecuted;

    uint32_t shutdownHandler;

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
