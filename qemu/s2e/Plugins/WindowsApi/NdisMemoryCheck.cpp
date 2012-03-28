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

#include <s2e/Utils.h>
#include "NdisHandlers.h"
#include "Ndis.h"

namespace s2e {
namespace plugins {

std::string NdisHandlers::makeConfigurationRegionString(uint32_t handle, bool free)
{
    std::stringstream ss;
    ss << "ndis:NdisReadConfiguration:" << hexval(handle);
    if (free) {
        ss << "*";
    } else {
        ss << ":";
    }
    return ss.str();
}

void NdisHandlers::grantPacket(S2EExecutionState *state, uint32_t pNdisPacket, uint32_t ProtocolReservedLength)
{
   if(!m_memoryChecker) {
       return;
   }

   std::stringstream ss;
   ss << "ndis:alloc:NDIS_PACKET:" << hexval(pNdisPacket);

   uint32_t size = sizeof(windows::NDIS_PACKET32) +
                   sizeof(windows::NDIS_PACKET_OOB_DATA32) +
                   sizeof(windows::NDIS_PACKET_EXTENSION32) +
                   ProtocolReservedLength;
   m_memoryChecker->grantMemory(state, pNdisPacket, size,
                             MemoryChecker::READWRITE,
                             ss.str());

   //Grant access rights to the list of buffers (MDLs)
   windows::MDL32 CurMdl;
   windows::NDIS_PACKET32 Packet;

   if (!state->readMemoryConcrete(pNdisPacket, &Packet, sizeof(Packet))) {
       return;
   }

   uint32_t head = Packet.Private.Head;
   while (head) {
       m_memoryChecker->grantMemory(state, head, sizeof(CurMdl),
                        MemoryChecker::READ, ss.str() + ":MDL");

       if (!state->readMemoryConcrete(head, &CurMdl, sizeof(CurMdl))) {
           break;
       }

       m_memoryChecker->grantMemory(state, CurMdl.StartVa + CurMdl.ByteOffset, CurMdl.ByteCount,
                        MemoryChecker::READWRITE, ss.str() + ":MDLBUF");

       head = CurMdl.Next;
   }
}

void NdisHandlers::revokePacket(S2EExecutionState *state, uint32_t pNdisPacket)
{
   if (!m_memoryChecker) {
       return;
   }

   std::stringstream ss;
   ss << "ndis:alloc:NDIS_PACKET:" << hexval(pNdisPacket) << "*";
   m_memoryChecker->revokeMemory(state, ss.str(), uint64_t(-1));

}

void NdisHandlers::grantMiniportAdapterContext(S2EExecutionState *state, uint32_t HandleParamNum)
{
   if (!m_memoryChecker) {
       return;
   }

   //Some entry points may be called internally. Don't grant in such cases.
   if (calledFromModule(state)) {
       return;
   }

   uint32_t NdisHandle;
   if (!readConcreteParameter(state, HandleParamNum, &NdisHandle)) {
       s2e()->getDebugStream(state) << "Could not read NdisHandle\n";
       return;
   }
   if (NdisHandle) {
       m_memoryChecker->grantMemory(state, NdisHandle + 0x150, 0x19c - 0x150,
                             MemoryChecker::READ,
                             "ndis:NDIS_MINIPORT_BLOCK:Callbacks");

       m_memoryChecker->grantMemory(state, NdisHandle + 0xec, 0xf8 - 0xec,
                             MemoryChecker::READ,
                             "ndis:NDIS_MINIPORT_BLOCK:Callbacks");

       m_memoryChecker->grantMemory(state, NdisHandle + 0xd8, 0xec - 0xd8,
                             MemoryChecker::READ,
                             "ndis:NDIS_MINIPORT_BLOCK:XFILTER");

   }
}

void NdisHandlers::revokeMiniportAdapterContext(S2EExecutionState *state)
{
    if (m_memoryChecker) {
        //Some entry points may be called internally. Don't revoke in such cases.
        //XXX: broken
        //if (calledFromModule(state)) {
        //   return;
        //}
        m_memoryChecker->revokeMemory(state, "ndis:NDIS_MINIPORT_BLOCK*");
    }
}

void NdisHandlers::grantBindingHandle(S2EExecutionState *state, uint32_t NdisBindingHandle)
{
    assert(false && "Not tested");
    if (!m_memoryChecker) {
       return;
    }

    //Some entry points may be called internally. Don't grant in such cases.
    if (calledFromModule(state)) {
       return;
    }

    if (!NdisBindingHandle) {
       return;
    }

    m_memoryChecker->grantMemory(state, NdisBindingHandle, windows::NDIS_OPEN_BLOCK_SIZE,
                         MemoryChecker::READ,
                         "ndis:NDIS_OPEN_BLOCK");
}

void NdisHandlers::revokeBindingHandle(S2EExecutionState *state)
{
    if (m_memoryChecker) {
        m_memoryChecker->revokeMemory(state, "ndis:NDIS_OPEN_BLOCK*");
    }
}

} //plugins
} //s2e
