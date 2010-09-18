#ifndef S2E_PLUGINS_NDISHANDLERS_H
#define S2E_PLUGINS_NDISHANDLERS_H

#include <s2e/Plugin.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>
#include <s2e/Plugins/SymbolicHardware.h>


#include <s2e/Plugins/StateManager.h>

namespace s2e {
namespace plugins {

#define NDIS_STATUS_SUCCESS 0
#define NDIS_STATUS_FAILURE 0xC0000001L
#define NDIS_STATUS_RESOURCES 0xc000009a
#define NDIS_STATUS_RESOURCE_CONFLICT 0xc001001E

#define OID_GEN_MEDIA_CONNECT_STATUS 0x00010114

#define REGISTER_NDIS_ENTRY_POINT(cs, pc, name) \
    if (pc) {\
        s2e()->getMessagesStream() << "Registering " # name <<  " at 0x" << std::hex << pc << std::endl; \
        cs = m_functionMonitor->getCallSignal(state, pc, 0); \
        cs->connect(sigc::mem_fun(*this, &NdisHandlers::name)); \
    }

#define DECLARE_NDIS_ENTRY_POINT(name) \
    void name(S2EExecutionState* state, FunctionMonitorState *fns); \
    void name##Ret(S2EExecutionState* state)

#define REGISTER_IMPORT(I, dll, name) \
    registerImport(I, dll, #name, &NdisHandlers::name, state)

class NdisHandlers : public Plugin
{
    S2E_PLUGIN
public:
    typedef std::set<std::string> StringSet;
    typedef void (NdisHandlers::*FunctionHandler)( S2EExecutionState* state, FunctionMonitorState *fns );

    NdisHandlers(S2E* s2e): Plugin(s2e) {}

    void initialize();

    static bool NtSuccess(S2E *s2e, S2EExecutionState *s, klee::ref<klee::Expr> &eq);

    static bool ReadUnicodeString(S2EExecutionState *state, uint32_t address, std::string &s);

    enum Consistency {
        STRICT, LOCAL, OVERAPPROX, OVERCONSTR
    };

private:
    FunctionMonitor *m_functionMonitor;
    OSMonitor *m_windowsMonitor;
    ModuleExecutionDetector *m_detector;
    StateManager *m_manager;
    SymbolicHardware *m_hw;

    //Modules we want to intercept
    StringSet m_modules;
    StringSet m_loadedModules;
    std::string m_hwId;
    DeviceDescriptor *m_devDesc;

    StringSet m_ignoreKeywords;

    Consistency m_consistency;

    void onModuleLoad(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void onModuleUnload(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        );

    bool calledFromModule(S2EExecutionState *s);

    void registerImport(Imports &I, const std::string &dll, const std::string &name,
                        FunctionHandler handler, S2EExecutionState *state);


    static bool readConcreteParameter(S2EExecutionState *s, unsigned param, uint32_t *val);
    static bool writeParameter(S2EExecutionState *s, unsigned param, klee::ref<klee::Expr> val);

    void TraceCall(S2EExecutionState* state, FunctionMonitorState *fns);
    void TraceRet(S2EExecutionState* state);

    DECLARE_NDIS_ENTRY_POINT(entryPoint);

    DECLARE_NDIS_ENTRY_POINT(NdisMRegisterMiniport);
    DECLARE_NDIS_ENTRY_POINT(NdisAllocateMemory);
    DECLARE_NDIS_ENTRY_POINT(NdisAllocateMemoryWithTag);
    DECLARE_NDIS_ENTRY_POINT(NdisMRegisterIoPortRange);
    DECLARE_NDIS_ENTRY_POINT(NdisMRegisterInterrupt);
    DECLARE_NDIS_ENTRY_POINT(NdisReadNetworkAddress);
    DECLARE_NDIS_ENTRY_POINT(NdisReadConfiguration);


    DECLARE_NDIS_ENTRY_POINT(RtlEqualUnicodeString);
    DECLARE_NDIS_ENTRY_POINT(GetSystemUpTime);
    DECLARE_NDIS_ENTRY_POINT(KeStallExecutionProcessor);

    DECLARE_NDIS_ENTRY_POINT(CheckForHang);
    DECLARE_NDIS_ENTRY_POINT(InitializeHandler);
    DECLARE_NDIS_ENTRY_POINT(DisableInterruptHandler);
    DECLARE_NDIS_ENTRY_POINT(EnableInterruptHandler);
    DECLARE_NDIS_ENTRY_POINT(HaltHandler);
    DECLARE_NDIS_ENTRY_POINT(HandleInterruptHandler);
    DECLARE_NDIS_ENTRY_POINT(ISRHandler);
    DECLARE_NDIS_ENTRY_POINT(QueryInformationHandler);
    DECLARE_NDIS_ENTRY_POINT(ReconfigureHandler);
    DECLARE_NDIS_ENTRY_POINT(ResetHandler);
    DECLARE_NDIS_ENTRY_POINT(SendHandler);
    DECLARE_NDIS_ENTRY_POINT(SendPacketsHandler);
    DECLARE_NDIS_ENTRY_POINT(SetInformationHandler);
    DECLARE_NDIS_ENTRY_POINT(TransferDataHandler);

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
    bool fakeoid;

    uint32_t isrRecognized, isrQueue;
    bool waitHandler;
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
