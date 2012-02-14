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

//#define __STDC_CONSTANT_MACROS 1
//#define __STDC_LIMIT_MACROS 1
//#define __STDC_FORMAT_MACROS 1


#include "ModuleParser.h"
#include <lib/BinaryReaders/Library.h>

#include <stdio.h>
#include <inttypes.h>
#include <string>
#include <sstream>
#include <iostream>

namespace s2etools {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ModuleInstance::ModuleInstance(
        const std::string &name, uint64_t pid, uint64_t loadBase, uint64_t size, uint64_t imageBase)
{
    LoadBase = loadBase;
    ImageBase = imageBase;
    Size = size;
    Name = name;
    //xxx: fix this
    Pid = pid;
}

void ModuleInstance::print(std::ostream &os) const
{
    os << "Instance of " << Name <<
            " Pid=0x" << std::hex << Pid <<
            " LoadBase=0x" << LoadBase << std::endl;
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


ModuleCache::ModuleCache(LogEvents *Events)
{
    Events->onEachItem.connect(
            sigc::mem_fun(*this, &ModuleCache::onItem)
            );

    m_events = Events;
}

void ModuleCache::onItem(unsigned traceIndex,
            const s2e::plugins::ExecutionTraceItemHeader &hdr,
            void *item)
{

    if (hdr.type == s2e::plugins::TRACE_MOD_LOAD) {
        const s2e::plugins::ExecutionTraceModuleLoad &load = *(s2e::plugins::ExecutionTraceModuleLoad*)item;
        ModuleCacheState *state = static_cast<ModuleCacheState*>(m_events->getState(this, &ModuleCacheState::factory));

        if (!state->loadModule(load.name, hdr.pid, load.loadBase, load.nativeBase, load.size)) {
            //std::cout << "Could not load driver " << load.name << std::endl;
        }
    }else if (hdr.type == s2e::plugins::TRACE_MOD_UNLOAD) {
        const s2e::plugins::ExecutionTraceModuleUnload &unload = *(s2e::plugins::ExecutionTraceModuleUnload*)item;
        ModuleCacheState *state = static_cast<ModuleCacheState*>(m_events->getState(this, &ModuleCacheState::factory));

        if (!state->unloadModule(hdr.pid, unload.loadBase)) {
            //std::cout << "Could not load driver " << load.name << std::endl;
        }
    }else if (hdr.type == s2e::plugins::TRACE_PROC_UNLOAD) {
        std::cerr << "Process unloading not implemented" << std::endl;
    }else {
        return;
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////



const ModuleInstance *ModuleCacheState::getInstance(uint64_t pid, uint64_t pc) const
{
    pid = Library::translatePid(pid, pc);

    ModuleInstance mi("", pid, pc, 1, 0);
    ModuleInstanceSet::const_iterator it = m_Instances.find(&mi);
    if (it == m_Instances.end()) {
        return NULL;
    }

    return (*it);
}


bool ModuleCacheState::loadModule(const std::string &name, uint64_t pid, uint64_t loadBase,
                             uint64_t imageBase, uint64_t size)
{
    std::cout << "Loading module " << name << " pid=0x" << std::hex << pid <<
            " loadBase=0x" << loadBase << " imageBase=0x" << imageBase << " size=0x" << size << std::endl;
    pid = Library::translatePid(pid, loadBase);

    ModuleInstance *mi = new ModuleInstance(name, pid, loadBase, size, imageBase);
    if(m_Instances.find(mi) != m_Instances.end()) {
        delete mi;
        return false;
    }
    m_Instances.insert(mi);
    return true;
}

bool ModuleCacheState::unloadModule(uint64_t pid, uint64_t loadBase)
{
    std::cout << "Unloading module pid=0x" << std::hex << pid <<
                 " loadBase=0x" << loadBase << "\n";

    pid = Library::translatePid(pid, loadBase);
    ModuleInstance mi("", pid, loadBase, 1, 0);

    //Sometimes we have duplicated items in the trace
    //assert(m_Instances.find(&mi) != m_Instances.end());

    return m_Instances.erase(&mi);
}



ItemProcessorState *ModuleCacheState::factory()
{
    return new ModuleCacheState();
}

ModuleCacheState::ModuleCacheState()
{

}

ModuleCacheState::~ModuleCacheState()
{

}

ItemProcessorState *ModuleCacheState::clone() const
{
    return new ModuleCacheState(*this);
}

}
