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

using namespace s2e::windows;

namespace s2e {
namespace plugins {

//XXX: all these functions should be compiled to LLVM and run in KLEE
//because all the data structures may contain symbolic data and handling it here is cumbersome

bool NdisHandlers::makePacketSymbolic(S2EExecutionState *s, uint32_t pPacket, bool keepSymbolicData)
{
    NDIS_PACKET32 Packet;

    if (!s->readMemoryConcrete(pPacket, &Packet, sizeof(Packet))) {
        s2e()->getWarningsStream() << "Could not read NDIS_PACKET. Maybe it has symbolic data?" << '\n';
        return false;
    }

    if (!Packet.Private.Head) {
        s2e()->getDebugStream() << "NDIS_PACKET " << hexval(pPacket) << " is empty." << '\n';
        return false;
    }

    uint32_t pCurrentBuffer;
    uint32_t pNextBuffer;

    for (pCurrentBuffer = Packet.Private.Head; pCurrentBuffer; pCurrentBuffer = pNextBuffer) {
        MDL32 CurrentMdl;
        if (!s->readMemoryConcrete(pCurrentBuffer, &CurrentMdl, sizeof(CurrentMdl))) {
            s2e()->getWarningsStream() << "Could not read NDIS_BUFFER. Maybe it has symbolic data?" << '\n';
            return false;
        }

        pNextBuffer = CurrentMdl.Next;

        if (!(CurrentMdl.MdlFlags & MDL_MAPPING_FLAGS)) {
            s2e()->getWarningsStream() << "NDIS_BUFFER MDL is not mapped" << '\n';
            continue;
        }


        for (unsigned i=0; i<CurrentMdl.ByteCount; ++i) {
            klee::ref<klee::Expr> data = s->readMemory(CurrentMdl.StartVa+i, klee::Expr::Int8);
            //XXX: Should avoid marking the buffer as symbolic several times
            //Need to keep track of these buffers somehow.
            if (!keepSymbolicData || isa<klee::ConstantExpr>(data)) {
                std::string varName = getVariableName(s, __FUNCTION__);
                data = s->createSymbolicValue(varName, klee::Expr::Int8);
                s->writeMemory(CurrentMdl.StartVa + i, data);
            }
        }
    }

    return true;
}

}
}
