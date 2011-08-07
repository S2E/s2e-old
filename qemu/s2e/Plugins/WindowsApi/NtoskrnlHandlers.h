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

#ifndef S2E_PLUGINS_NTOSKRNLHANDLERS_H
#define S2E_PLUGINS_NTOSKRNLHANDLERS_H

#include <s2e/Plugin.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>
#include <s2e/Plugins/SymbolicHardware.h>

#include "Api.h"


namespace s2e {
namespace plugins {

#define NTOSKRNL_REGISTER_ENTRY_POINT(addr, ep) registerEntryPoint<NtoskrnlHandlers, NtoskrnlHandlers::EntryPoint>(state, this, &NtoskrnlHandlers::ep, addr);

class NtoskrnlHandlers: public WindowsApi
{
    S2E_PLUGIN
public:
    typedef void (NtoskrnlHandlers::*EntryPoint)(S2EExecutionState* state, FunctionMonitorState *fns);
    typedef std::map<std::string, NtoskrnlHandlers::EntryPoint> NtoskrnlHandlersMap;

    NtoskrnlHandlers(S2E* s2e): WindowsApi(s2e) {}

    void initialize();

public:
    static const WindowsApiHandler<EntryPoint> s_handlers[];
    static const NtoskrnlHandlersMap s_handlersMap;

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

    DECLARE_ENTRY_POINT(DebugPrint);
    DECLARE_ENTRY_POINT(IoCreateSymbolicLink);
    DECLARE_ENTRY_POINT(IoCreateDevice);
    DECLARE_ENTRY_POINT(IoIsWdmVersionAvailable);
    DECLARE_ENTRY_POINT_CO(IoFreeMdl);

    DECLARE_ENTRY_POINT(RtlEqualUnicodeString);
    DECLARE_ENTRY_POINT(RtlAddAccessAllowedAce);
    DECLARE_ENTRY_POINT(RtlCreateSecurityDescriptor);
    DECLARE_ENTRY_POINT(RtlSetDaclSecurityDescriptor);
    DECLARE_ENTRY_POINT(RtlAbsoluteToSelfRelativeSD);


    DECLARE_ENTRY_POINT(GetSystemUpTime);
    DECLARE_ENTRY_POINT(KeStallExecutionProcessor);

    DECLARE_ENTRY_POINT(ExAllocatePoolWithTag, uint32_t poolType, uint32_t size);

public:
    DECLARE_ENTRY_POINT_CALL(DriverDispatch, uint32_t irpMajor);
};

}

namespace windows {
    static const uint32_t STATUS_SUCCESS = 0;
    static const uint32_t STATUS_PENDING = 0x00000103;
    static const uint32_t STATUS_BUFFER_TOO_SMALL = 0xC0000023;
    static const uint32_t STATUS_UNKNOWN_REVISION = 0xC0000058;
    static const uint32_t STATUS_INVALID_SECURITY_DESCR = 0xC0000079;
    static const uint32_t STATUS_BAD_DESCRIPTOR_FORMAT = 0xC00000E7;

    typedef uint32_t PACL32;
    typedef uint32_t PSID32;
    typedef uint16_t SECURITY_DESCRIPTOR_CONTROL;
    typedef uint32_t PDEVICE_OBJECT32;

    struct SECURITY_DESCRIPTOR32 {
        uint8_t Revision;
        uint8_t Sbz1;
        SECURITY_DESCRIPTOR_CONTROL Control;
        PSID32 Owner;
        PSID32 Group;
        PACL32 Sacl;
        PACL32 Dacl;
    }__attribute__((packed));


    static const uint32_t  IRP_MJ_CREATE                     = 0x00;
    static const uint32_t  IRP_MJ_CREATE_NAMED_PIPE          = 0x01;
    static const uint32_t  IRP_MJ_CLOSE                      = 0x02;
    static const uint32_t  IRP_MJ_READ                       = 0x03;
    static const uint32_t  IRP_MJ_WRITE                      = 0x04;
    static const uint32_t  IRP_MJ_QUERY_INFORMATION          = 0x05;
    static const uint32_t  IRP_MJ_SET_INFORMATION            = 0x06;
    static const uint32_t  IRP_MJ_QUERY_EA                   = 0x07;
    static const uint32_t  IRP_MJ_SET_EA                     = 0x08;
    static const uint32_t  IRP_MJ_FLUSH_BUFFERS              = 0x09;
    static const uint32_t  IRP_MJ_QUERY_VOLUME_INFORMATION   = 0x0a;
    static const uint32_t  IRP_MJ_SET_VOLUME_INFORMATION     = 0x0b;
    static const uint32_t  IRP_MJ_DIRECTORY_CONTROL          = 0x0c;
    static const uint32_t  IRP_MJ_FILE_SYSTEM_CONTROL        = 0x0d;
    static const uint32_t  IRP_MJ_DEVICE_CONTROL             = 0x0e;
    static const uint32_t  IRP_MJ_INTERNAL_DEVICE_CONTROL    = 0x0f;
    static const uint32_t  IRP_MJ_SCSI                       = 0x0f;
    static const uint32_t  IRP_MJ_SHUTDOWN                   = 0x10;
    static const uint32_t  IRP_MJ_LOCK_CONTROL               = 0x11;
    static const uint32_t  IRP_MJ_CLEANUP                    = 0x12;
    static const uint32_t  IRP_MJ_CREATE_MAILSLOT            = 0x13;
    static const uint32_t  IRP_MJ_QUERY_SECURITY             = 0x14;
    static const uint32_t  IRP_MJ_SET_SECURITY               = 0x15;
    static const uint32_t  IRP_MJ_POWER                      = 0x16;
    static const uint32_t  IRP_MJ_SYSTEM_CONTROL             = 0x17;
    static const uint32_t  IRP_MJ_DEVICE_CHANGE              = 0x18;
    static const uint32_t  IRP_MJ_QUERY_QUOTA                = 0x19;
    static const uint32_t  IRP_MJ_SET_QUOTA                  = 0x1a;
    static const uint32_t  IRP_MJ_PNP                        = 0x1b;
    static const uint32_t  IRP_MJ_PNP_POWER                  = 0x1b;
    static const uint32_t  IRP_MJ_MAXIMUM_FUNCTION           = 0x1b;



}


}

#endif
