#ifndef S2E_PLUGINS_HALHANDLERS_H
#define S2E_PLUGINS_HALHANDLERS_H

#include <s2e/Plugin.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>

#include <s2e/Plugins/SymbolicHardware.h>

#include "Api.h"

#include <ostream>


namespace s2e {
namespace plugins {

class HalHandlers: public WindowsApi
{
    S2E_PLUGIN
public:
    HalHandlers(S2E* s2e): WindowsApi(s2e) {}

    void initialize();

private:
    bool m_loaded;
    ModuleDescriptor m_module;

    void onModuleLoad(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void onModuleUnload(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        );

    DECLARE_ENTRY_POINT(HalpValidPciSlot);

};

enum BUS_DATA_TYPE {
  ConfigurationSpaceUndefined = -1,
  Cmos,
  EisaConfiguration,
  Pos,
  CbusConfiguration,
  PCIConfiguration,
  VMEConfiguration,
  NuBusConfiguration,
  PCMCIAConfiguration,
  MPIConfiguration,
  MPSAConfiguration,
  PNPISAConfiguration,
  MaximumBusDataType
};


//
// HAL Bus Handler
//
struct BUS_HANDLER32
{
    uint32_t Version;
    s2e::windows::INTERFACE_TYPE InterfaceType;
    BUS_DATA_TYPE ConfigurationType;
    uint32_t BusNumber;
    uint32_t DeviceObject; //PDEVICE_OBJECT
    uint32_t ParentHandler; //struct _BUS_HANDLER *
    uint32_t BusData;  //PVOID
    uint32_t DeviceControlExtensionSize;
    //PSUPPORTED_RANGES BusAddresses;
    uint32_t Reserved[4];
#if 0
    pGetSetBusData GetBusData;
    pGetSetBusData SetBusData;
    pAdjustResourceList AdjustResourceList;
    pAssignSlotResources AssignSlotResources;
    pGetInterruptVector GetInterruptVector;
    pTranslateBusAddress TranslateBusAddress;
#endif
    void print(std::ostream &os) const {
        os << std::hex << "Version:           0x" << std::hex << Version << std::endl;
        os << std::hex << "InterfaceType:     0x" << std::hex << InterfaceType << std::endl;
        os << std::hex << "ConfigurationType: 0x" << std::hex << ConfigurationType << std::endl;
        os << std::hex << "BusNumber:         0x" << std::hex << BusNumber << std::endl;
        os << std::hex << "DeviceObject:      0x" << std::hex << DeviceObject << std::endl;
        os << std::hex << "ParentHandler:     0x" << std::hex << ParentHandler << std::endl;
        os << std::hex << "BusData:           0x" << std::hex << BusData << std::endl;        
    }
};

}
}

#endif
