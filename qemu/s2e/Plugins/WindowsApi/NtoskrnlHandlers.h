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

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>

#include "Api.h"
#include "Ntddk.h"


namespace s2e {
namespace plugins {

#define NTOSKRNL_REGISTER_ENTRY_POINT(addr, ep) registerEntryPoint(state, this, &NtoskrnlHandlers::ep, addr);

class NtoskrnlHandlersState;

class NtoskrnlHandlers: public WindowsAnnotations<NtoskrnlHandlers, NtoskrnlHandlersState >
{
    S2E_PLUGIN
public:
    NtoskrnlHandlers(S2E* s2e): WindowsAnnotations<NtoskrnlHandlers, NtoskrnlHandlersState >(s2e) {}

    void initialize();

public:
    static const WindowsApiHandler<Annotation> s_handlers[];
    static const AnnotationsMap s_handlersMap;

    static const char *s_ignoredFunctionsList[];
    static const StringSet s_ignoredFunctions;

    static const SymbolDescriptor s_exportedVariablesList[];
    static const SymbolDescriptors s_exportedVariables;

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
    DECLARE_ENTRY_POINT(IoCreateDevice, uint32_t pDeviceObject);
    DECLARE_ENTRY_POINT(IoDeleteDevice);
    DECLARE_ENTRY_POINT(IoIsWdmVersionAvailable);
    DECLARE_ENTRY_POINT_CO(IoFreeMdl);

    DECLARE_ENTRY_POINT(RtlEqualUnicodeString);
    DECLARE_ENTRY_POINT(RtlAddAccessAllowedAce);
    DECLARE_ENTRY_POINT(RtlCreateSecurityDescriptor);
    DECLARE_ENTRY_POINT(RtlSetDaclSecurityDescriptor);
    DECLARE_ENTRY_POINT(RtlAbsoluteToSelfRelativeSD);


    DECLARE_ENTRY_POINT(GetSystemUpTime);
    DECLARE_ENTRY_POINT(KeStallExecutionProcessor);

    DECLARE_ENTRY_POINT(MmGetSystemRoutineAddress);

    DECLARE_ENTRY_POINT(ExAllocatePoolWithTag, uint32_t poolType, uint32_t size);
    DECLARE_ENTRY_POINT(ExFreePool);
    DECLARE_ENTRY_POINT(ExFreePoolWithTag);

    DECLARE_ENTRY_POINT(IofCompleteRequest);

    static uint32_t IoGetCurrentIrpStackLocation(const windows::IRP *Irp) {
        return ( (Irp)->Tail.Overlay.CurrentStackLocation );
    }

    static std::string readUnicodeString(S2EExecutionState *state, uint32_t pUnicodeString);

    void grantAccessToIrp(S2EExecutionState *state, uint32_t pIrp);
    void revokeAccessToIrp(S2EExecutionState *state, uint32_t pIrp);

private:
    void DispatchIoctl(S2EExecutionState* state, uint64_t pIrp, const windows::IRP &irp,
                       const windows::IO_STACK_LOCATION &stackLocation);

    void DispatchWrite(S2EExecutionState* state, uint64_t pIrp, const windows::IRP &irp,
                       const windows::IO_STACK_LOCATION &stackLocation);

public:
    DECLARE_ENTRY_POINT_CALL(DriverDispatch, uint32_t irpMajor);
    DECLARE_ENTRY_POINT_RET(DriverDispatch, uint32_t irpMajor, bool isFake, uint32_t pIrp);
};


class NtoskrnlHandlersState: public WindowsApiState<NtoskrnlHandlers>
{
private:
    bool isFakeState;
    bool isIoctlIrpExplored;

public:
    NtoskrnlHandlersState(){
        isFakeState = false;
        isIoctlIrpExplored = false;
    }

    virtual ~NtoskrnlHandlersState(){};
    virtual NtoskrnlHandlersState* clone() const {
        return new NtoskrnlHandlersState(*this);
    }

    static PluginState *factory(Plugin *p, S2EExecutionState *s) {
        return new NtoskrnlHandlersState();
    }

    friend class NtoskrnlHandlers;
};


}
}

#endif
