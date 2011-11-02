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
#include "config.h"
#include "qemu-common.h"
}

#include "HostFiles.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <llvm/System/Path.h>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(HostFiles, "Access to host files", "",);

void HostFiles::initialize()
{
    //m_allowWrite = s2e()->getConfig()->getBool(
    //            getConfigKey() + ".allowWrite");
    m_baseDir = s2e()->getConfig()->getString(
                getConfigKey() + ".baseDir");
    if(m_baseDir.empty())
        m_baseDir = s2e()->getOutputDirectory();

    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &HostFiles::onCustomInstruction));
}

void HostFiles::onCustomInstruction(S2EExecutionState *state, uint64_t opcode)
{
    //XXX: find a better way of allocating custom opcodes
    if (!(((opcode>>8) & 0xFF) == 0xEE)) {
        return;
    }

    opcode >>= 16;
    uint8_t op = opcode & 0xFF;
    opcode >>= 8;

    switch(op) {
    case 0: // open
        {
            uint32_t fnamePtr, flags;
            uint32_t guestFd = (uint32_t) -1;
            bool ok = true;
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]), &fnamePtr, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]), &flags, 4);

            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &guestFd, 4);

            if (!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op HostFiles "
                    << std::endl;
                break;
            }

            std::string fname;
            if(!state->readString(fnamePtr, fname)) {
                s2e()->getWarningsStream(state)
                    << "Error reading file name string from the guest" << std::endl;
                break;
            }

            unsigned i;
            for(i = 0; i < fname.size(); ++i) {
                //Allow only certain characters in the file name.
                if(!(isalnum(fname[i]) || fname[i] == ','
                        || fname[i] == '_' || fname[i] == '-' || fname[i] == '.')) {
                    s2e()->getWarningsStream(state) <<
                            "HostFiles: Invalid character " << fname[i] << " in " << fname << std::endl;
                    break;
                }
            }

            if(fname.size() == 0 || i != fname.size()) {
                s2e()->getWarningsStream(state)
                        << "Guest passed invalid file name to HostFiles plugin: " << fname << std::endl;
                break;
            }

            llvm::sys::Path path(m_baseDir);
            path.appendComponent(fname);

            int oflags = O_RDONLY;
#ifdef CONFIG_WIN32
            oflags |= O_BINARY;
#endif

            int fd = open(path.c_str(), oflags);
            if(fd != -1) {
                m_openFiles.push_back(fd);
                guestFd = m_openFiles.size()-1;
                state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &guestFd, 4);
            }else {
                s2e()->getWarningsStream(state) <<
                        "HostFiles could not open " << path.c_str() << "(errno " << errno << ")" << std::endl;
            }
        }

        break;

    case 1: // close
        {
            uint32_t guestFd;
            uint32_t ret = (uint32_t) -1;

            bool ok = true;
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]), &guestFd, 4);

            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &ret, 4);

            if (!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op HostFiles "
                    << std::endl;
                break;
            }

            if(guestFd < m_openFiles.size() && m_openFiles[guestFd] != -1) {
                ret = close(m_openFiles[guestFd]);
                m_openFiles[guestFd] = -1;
                state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &ret, 4);
            }
        }

        break;

    case 2: // read
        {
            uint32_t guestFd, bufAddr, count;
            uint32_t ret = (uint32_t) -1;

            bool ok = true;
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]), &guestFd, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]), &bufAddr, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EDX]), &count, 4);

            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &ret, 4);

            if (!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op HostFiles" << std::endl;
                break;
            }

            if(count > 1024*64) {
                s2e()->getWarningsStream(state)
                    << "ERROR: count passed to HostFiles is too big" << std::endl;
                break;
            }

            if(guestFd > m_openFiles.size() || m_openFiles[guestFd] == -1) {
                break;
            }

            int fd = m_openFiles[guestFd];
            char buf[count];

            ret = read(fd, buf, count);
            if(ret == (uint32_t) -1)
                break;

            ok = state->writeMemoryConcrete(bufAddr, buf, ret);
            if(!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: HostFiles can not write to guest buffer" << std::endl;
                break;
            }

            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &ret, 4);
        }

        break;

    //case 3: // write
    //    break;

    default:
        s2e()->getWarningsStream(state)
                << "Invalid HostFiles opcode 0x"
                << std::hex << op << std::dec << std::endl;
        break;
    }
}


} // namespace plugins
} // namespace s2e

