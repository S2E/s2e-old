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

#ifndef _NT_DDK_H_

#define _NT_DDK_H_

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>

namespace s2e {
namespace windows {

struct MDL32 {
    uint32_t Next; //struct _MDL *
    CSHORT Size;
    CSHORT MdlFlags;
    uint32_t Process; //struct _EPROCESS *
    uint32_t MappedSystemVa;
    uint32_t StartVa;
    ULONG ByteCount;
    ULONG ByteOffset;
};

static const uint32_t MDL_MAPPED_TO_SYSTEM_VA     = 0x0001;
static const uint32_t MDL_PAGES_LOCKED            = 0x0002;
static const uint32_t MDL_SOURCE_IS_NONPAGED_POOL = 0x0004;
static const uint32_t MDL_ALLOCATED_FIXED_SIZE    = 0x0008;
static const uint32_t MDL_PARTIAL                 = 0x0010;
static const uint32_t MDL_PARTIAL_HAS_BEEN_MAPPED = 0x0020;
static const uint32_t MDL_IO_PAGE_READ            = 0x0040;
static const uint32_t MDL_WRITE_OPERATION         = 0x0080;
static const uint32_t MDL_PARENT_MAPPED_SYSTEM_VA = 0x0100;
static const uint32_t MDL_FREE_EXTRA_PTES         = 0x0200;
static const uint32_t MDL_IO_SPACE                = 0x0800;
static const uint32_t MDL_NETWORK_HEADER          = 0x1000;
static const uint32_t MDL_MAPPING_CAN_FAIL        = 0x2000;
static const uint32_t MDL_ALLOCATED_MUST_SUCCEED  = 0x4000;


static const uint32_t MDL_MAPPING_FLAGS = (MDL_MAPPED_TO_SYSTEM_VA     |
                           MDL_PAGES_LOCKED            |
                           MDL_SOURCE_IS_NONPAGED_POOL |
                           MDL_PARTIAL_HAS_BEEN_MAPPED |
                           MDL_PARENT_MAPPED_SYSTEM_VA |
                    //       MDL_SYSTEM_VA               |
                           MDL_IO_SPACE );

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
