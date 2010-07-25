#ifndef _NDIS_H_

#define _NDIS_H_

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>

namespace s2e {
namespace windows {


#define NDIS_ERROR_CODE unsigned long
typedef uint32_t NDIS_HANDLE, *PNDIS_HANDLE;

typedef int NDIS_STATUS, *PNDIS_STATUS; // note default size

typedef struct _NDIS_MINIPORT_CHARACTERISTICS32 {
    uint8_t MajorNdisVersion;
    uint8_t MinorNdisVersion;
    uint32_t Reserved;
    uint32_t CheckForHangHandler;
    uint32_t DisableInterruptHandler;
    uint32_t EnableInterruptHandler;
    uint32_t HaltHandler;
    uint32_t HandleInterruptHandler;
    uint32_t InitializeHandler;
    uint32_t ISRHandler;
    uint32_t QueryInformationHandler;
    uint32_t ReconfigureHandler;
    uint32_t ResetHandler;
    uint32_t SendHandler; 
    uint32_t SetInformationHandler;
    uint32_t TransferDataHandler;
//
// Version used is V4.0 or V5.0
// with following members
//
    uint32_t ReturnPacketHandler;
    uint32_t SendPacketsHandler;
    uint32_t AllocateCompleteHandler;
//
// Version used is V5.0 with the following members
//
    uint32_t CoCreateVcHandler;
    uint32_t CoDeleteVcHandler;
    uint32_t CoActivateVcHandler;
    uint32_t CoDeactivateVcHandler;
    uint32_t CoSendPacketsHandler;
    uint32_t CoRequestHandler;
//
// Version used is V5.1 with the following members
//
    uint32_t CancelSendPacketsHandler;
    uint32_t PnPEventNotifyHandler;
    uint32_t AdapterShutdownHandler;
} NDIS_MINIPORT_CHARACTERISTICS32, *PNDIS_MINIPORT_CHARACTERISTICS32;

typedef enum _NDIS_MEDIUM {
  NdisMedium802_3,
  NdisMedium802_5,
  NdisMediumWan,
  NdisMediumDix,
  NdisMediumWirelessWan,
  NdisMediumIrda,
  NdisMediumBpc,
  NdisMediumCoWan,
  NdisMedium1394,
  NdisMediumMax,
} NDIS_MEDIUM, *PNDIS_MEDIUM;

typedef
NDIS_STATUS
(*W_INITIALIZE_HANDLER)(
    PNDIS_STATUS            OpenErrorStatus,  //OUT
    /*PUINT*/ uint32_t                   SelectedMediumIndex, //OUT
    PNDIS_MEDIUM            MediumArray,  
    uint32_t                    MediumArraySize,
    NDIS_HANDLE             MiniportAdapterContext,
    NDIS_HANDLE             WrapperConfigurationContext
    );




}
}


#endif
