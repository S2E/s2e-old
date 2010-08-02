//#define __STDC_CONSTANT_MACROS 1
//#define __STDC_LIMIT_MACROS 1
//#define __STDC_FORMAT_MACROS 1


#include "ModuleParser.h"

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

    const s2e::plugins::ExecutionTraceModuleLoad &load = *(s2e::plugins::ExecutionTraceModuleLoad*)item;

    if (hdr.type == s2e::plugins::TRACE_MOD_LOAD) {
        ModuleCacheState *state = static_cast<ModuleCacheState*>(m_events->getState(this, &ModuleCacheState::factory));

        if (!state->loadDriver(load.name, hdr.pid, load.loadBase, load.nativeBase, load.size)) {
            //std::cout << "Could not load driver " << load.name << std::endl;
        }
    }else if (hdr.type == s2e::plugins::TRACE_MOD_UNLOAD) {
        std::cerr << "Module unloading not implemented" << std::endl;
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
    ModuleInstance mi("", pid, pc, 1, 0);
    ModuleInstanceSet::const_iterator it = m_Instances.find(&mi);
    if (it == m_Instances.end()) {
        return NULL;
    }

    return (*it);
}


bool ModuleCacheState::loadDriver(const std::string &name, uint64_t pid, uint64_t loadBase,
                             uint64_t imageBase, uint64_t size)
{
    ModuleInstance *mi = new ModuleInstance(name, pid, loadBase, size, imageBase);
    assert(m_Instances.find(mi) == m_Instances.end());
    m_Instances.insert(mi);
    return true;
}

bool ModuleCacheState::unloadDriver(uint64_t pid, uint64_t loadBase)
{
    ModuleInstance mi("", pid, loadBase, 1, 0);
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
