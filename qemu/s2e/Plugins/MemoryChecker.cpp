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

#include "MemoryChecker.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <s2e/S2EExecutor.h>
#include <s2e/Plugins/ModuleDescriptor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/OSMonitor.h>

#include <klee/Internal/ADT/ImmutableMap.h>

#include <iostream>
#include <sstream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(MemoryChecker, "MemoryChecker plugin", "",
                  "Interceptor", "ModuleExecutionDetector");

namespace {
    struct MemoryRange {
        uint64_t start;
        uint64_t size;
    };

    struct MemoryRegion {
        MemoryRange range;
        uint8_t perms;
        uint64_t allocPC;
        std::string type;
        uint64_t id;
        bool permanent;
    };

    struct MemoryRangeLT {
        bool operator()(const MemoryRange& a, const MemoryRange& b) const {
            return a.start + a.size <= b.start;
        }
    };

    typedef klee::ImmutableMap<MemoryRange, const MemoryRegion*,
                               MemoryRangeLT> MemoryMap;

    std::ostream& operator <<(std::ostream& out, const MemoryRegion& r) {
        out << "MemoryRegion(\n"
            << "    start = " << hexval(r.range.start) << "\n"
            << "    size = " << hexval(r.range.size) << "\n"
            << "    perms = " << hexval(r.perms) << "\n"
            << "    allocPC = " << hexval(r.allocPC) << "\n"
            << "    type = " << r.type << "\n"
            << "    id = " << hexval(r.id) << "\n"
            << "    permanent = " << r.permanent << ")";
        return out;
    }
} // namespace

class MemoryCheckerState: public PluginState
{
public:
    //const ModuleDescriptor* module;

    //***** XXX! *****
    // For some reason OSMonitor sometimes gives different ModuleDescriptors
    // for the same module. As a workaroung we compare LoadBase instead.
    ModuleDescriptor m_module;

    MemoryMap m_memoryMap;

public:
    MemoryCheckerState() { m_module.LoadBase = 0; }
    ~MemoryCheckerState() {}

    MemoryCheckerState *clone() const { return new MemoryCheckerState(*this); }
    static PluginState *factory(Plugin*, S2EExecutionState*) {
        return new MemoryCheckerState();
    }

    MemoryMap* getModuleMemoryMap(const ModuleDescriptor* module) {
#warning Checking the module is currently disabled because limitations of NDisHandlers
        return &m_memoryMap;
        /*
        if(module->LoadBase == m_module.LoadBase)
            return &m_memoryMap;
        else
            return 0;
        */
    }

    void setModuleMemoryMap(const ModuleDescriptor* module, const MemoryMap& memoryMap) {
        //assert(module->LoadBase == m_module.LoadBase);
        m_memoryMap = memoryMap;
    }
};

void MemoryChecker::initialize()
{
    ConfigFile *cfg = s2e()->getConfig();
    ConfigFile::string_list mods = cfg->getStringList(getConfigKey() + ".moduleIds");
    if (mods.size() == 0) {
        s2e()->getWarningsStream() << "MemoryChecker: No modules to track configured for the MemoryChecker plugin" << std::endl;
        exit(-1);
        return;
    }

    assert(mods.size() == 1 && "Only one module is currently supported!");
    m_moduleId = mods[0];

    m_checkMemoryErrors = cfg->getBool(getConfigKey() + ".checkMemoryErrors");
    m_checkMemoryLeaks = cfg->getBool(getConfigKey() + ".checkMemoryLeaks");

    m_terminateOnErrors = cfg->getBool(getConfigKey() + ".terminateOnErrors");
    m_terminateOnLeaks = cfg->getBool(getConfigKey() + ".terminateOnLeaks");

    m_moduleDetector = static_cast<ModuleExecutionDetector*>(
                            s2e()->getPlugin("ModuleExecutionDetector"));
    assert(m_moduleDetector);

    m_osMonitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));
    assert(m_osMonitor);

    m_osMonitor->onModuleLoad.connect(
            sigc::mem_fun(*this,
                    &MemoryChecker::onModuleLoad)
            );

    m_osMonitor->onModuleUnload.connect(
            sigc::mem_fun(*this,
                    &MemoryChecker::onModuleUnload)
            );

    if(m_checkMemoryErrors) {
        m_moduleDetector->onModuleTransition.connect(
                sigc::mem_fun(*this,
                        &MemoryChecker::onModuleTransition)
                );
    }
}

void MemoryChecker::onModuleLoad(S2EExecutionState* state,
                                 const ModuleDescriptor &module)
{
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    const std::string* moduleId = m_moduleDetector->getModuleId(module);
    if(moduleId && *moduleId == m_moduleId) {
        if(!plgState->m_module.LoadBase) {
            plgState->m_module = module;
        } else {
            assert(plgState->m_module.LoadBase == module.LoadBase);
        }
    }
}

void MemoryChecker::onModuleUnload(S2EExecutionState* state,
                    const ModuleDescriptor &module)
{
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    if(plgState->m_module.LoadBase == module.LoadBase) {
        plgState->m_module.LoadBase = 0;
    }
}

void MemoryChecker::onModuleTransition(S2EExecutionState *state,
                                       const ModuleDescriptor *prevModule,
                                       const ModuleDescriptor *nextModule)
{
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    if(nextModule && nextModule->LoadBase == plgState->m_module.LoadBase) {
        m_dataMemoryAccessConnection =
            s2e()->getCorePlugin()->onDataMemoryAccess.connect(
                sigc::mem_fun(*this, &MemoryChecker::onDataMemoryAccess)
            );
    } else {
        m_dataMemoryAccessConnection.disconnect();
    }
}

void MemoryChecker::onDataMemoryAccess(S2EExecutionState *state,
                                       klee::ref<klee::Expr> virtualAddress,
                                       klee::ref<klee::Expr> hostAddress,
                                       klee::ref<klee::Expr> value,
                                       bool isWrite, bool isIO)
{
    if(!isa<klee::ConstantExpr>(virtualAddress)) {
        s2e()->getWarningsStream(state) << "Symbolic memory accesses are "
                << "not yet supported by MemoryChecker" << std::endl;
        return;
    }

    DECLARE_PLUGINSTATE(MemoryCheckerState, state);
    uint64_t start = cast<klee::ConstantExpr>(virtualAddress)->getZExtValue();
    checkMemoryAccess(state, &plgState->m_module, start,
                      klee::Expr::getMinBytesForWidth(value->getWidth()),
                      isWrite ? 2 : 1);
}

bool MemoryChecker::matchRegionType(const std::string &pattern, const std::string &type)
{
    if(pattern.size() == 0)
        return true;

    if(type.size() == 0)
        return pattern[0] == '*' && pattern[1] == 0;

    size_t len = pattern.size();

    if(len == 0) // corner case
        return type[0] == 0;

    if(pattern[len-1] != '*')
        return pattern.compare(type) == 0;

    return type.compare(pattern.substr(0, len-1)) == 0;
}

void MemoryChecker::grantMemory(S2EExecutionState *state,
                                const ModuleDescriptor *module,
                                uint64_t start, uint64_t size, uint8_t perms,
                                const std::string &regionType, uint64_t regionID,
                                bool permanent)
{
    // XXX: ugh, this is really ugly!
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    MemoryMap *memoryMap = plgState->getModuleMemoryMap(module);
    if(!memoryMap)
        return;

    MemoryRegion *region = new MemoryRegion();
    region->range.start = start;
    region->range.size = size;
    region->perms = perms;
    region->allocPC = state->getPc();
    region->type = regionType;
    region->id = regionID;
    region->permanent = permanent;

    s2e()->getDebugStream(state) << "MemoryChecker::grantMemory("
            << *region << ")" << std::endl;

    if(size == 0 || start + size < start) {
        s2e()->getWarningsStream(state) << "MemoryChecker::grantMemory: "
            << "detected region of " << (size == 0 ? "zero" : "negative")
            << " size!" << std::endl
            << "This probably means a bug in the OS or S2E API annotations" << std::endl;
        delete region;
        return;
    }

    const MemoryMap::value_type *res = memoryMap->lookup_previous(region->range);
    if (res && res->first.start + res->first.size > start) {
        s2e()->getWarningsStream(state) << "MemoryChecker::grantMemory: "
            << "detected overlapping ranges!" << std::endl
            << "This probably means a bug in the OS or S2E API annotations" << std::endl
            << "NOTE: requested region: " << *region << std::endl
            << "NOTE: overlapping region: " << *res->second << std::endl;
        delete region;
        return;
    }

    plgState->setModuleMemoryMap(module,
                memoryMap->replace(std::make_pair(region->range, region)));

}

bool MemoryChecker::revokeMemory(S2EExecutionState *state,
                                 const ModuleDescriptor *module,
                                 uint64_t start, uint64_t size, uint8_t perms,
                                 const std::string &regionTypePattern, uint64_t regionID)
{
    // XXX: ugh, this is really ugly!
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    MemoryMap *memoryMap = plgState->getModuleMemoryMap(module);
    if(!memoryMap)
        return true;

    MemoryRegion *region = new MemoryRegion();
    region->range.start = start;
    region->range.size = size;
    region->perms = perms;
    region->allocPC = state->getPc();
    region->type = regionTypePattern;
    region->id = regionID;

    s2e()->getDebugStream(state) << "MemoryChecker::revokeMemory("
            << *region << ")" << std::endl;

    std::ostringstream err;

    do {
        if(size != uint64_t(-1) && start + size < start) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing region of " << (size == 0 ? "zero" : "negative")
                << " size!" << std::endl;
            break;
        }

        const MemoryMap::value_type *res = memoryMap->lookup_previous(region->range);
        if(!res || res->first.start + res->first.size <= start) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory that was not allocated!" << std::endl;
            break;
        }

        if(res->first.start != start) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory that was not allocated!" << std::endl
                << "  NOTE: overlapping region exists: " << *res->second << std::endl
                << "  NOTE: requested region: " << *region << std::endl;
            break;
        }

        if(size != uint64_t(-1) && res->first.size != size) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory region of wrong size!" << std::endl
                << "  NOTE: allocated region: " << *res->second << std::endl
                << "  NOTE: requested region: " << *region << std::endl;
        }

        if(perms != uint8_t(-1) && res->second->perms != perms) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory region with wrong permissions!" << std::endl
                << "  NOTE: allocated region: " << *res->second << std::endl
                << "  NOTE: requested region: " << *region << std::endl;
        }

        if(regionTypePattern.size()>0 && !matchRegionType(regionTypePattern, res->second->type)) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory region with wrong region type!" << std::endl
                << "  NOTE: allocated region: " << *res->second << std::endl
                << "  NOTE: requested region: " << *region << std::endl;
        }

        if(regionID != uint64_t(-1) && res->second->id != regionID) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory region with wrong region ID!" << std::endl
                << "  NOTE: allocated region: " << *res->second << std::endl
                << "  NOTE: requested region: " << *region << std::endl;
        }

        //we can not just delete it since it can be used by other states!
        //delete const_cast<MemoryRegion*>(res->second);
        plgState->setModuleMemoryMap(module, memoryMap->remove(region->range));
    } while(false);

    delete region;

    if(!err.str().empty()) {
        if(m_terminateOnErrors)
            s2e()->getExecutor()->terminateStateEarly(*state, err.str());
        else
            s2e()->getWarningsStream(state) << err.str() << std::flush;
        return false;
    }

    return true;
}

bool MemoryChecker::revokeMemory(S2EExecutionState *state,
                                 const ModuleDescriptor *module,
                                 const std::string &regionTypePattern, uint64_t regionID)
{
    // XXX: ugh, this is really ugly!
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    MemoryMap *memoryMap = plgState->getModuleMemoryMap(module);
    if(!memoryMap)
        return true;

    s2e()->getDebugStream(state) << "MemoryChecker::revokeMemory("
            << "pattern = '" << regionTypePattern << "', "
            << "regionID = " << hexval(regionID) << ")" << std::endl;

    bool ret = true;
    bool changed = true;
    while(changed) {
        changed = false;
        for(MemoryMap::iterator it = memoryMap->begin(), ie = memoryMap->end();
                                                        it != ie; ++it) {
            if(it->second->type.size()>0
                  && matchRegionType(regionTypePattern, it->second->type)
                  && (regionID == uint64_t(-1) || it->second->id == regionID)) {
                ret &= revokeMemory(state, module,
                             it->first.start, it->first.size,
                             it->second->perms, it->second->type, it->second->id);
                changed = true;
                memoryMap = plgState->getModuleMemoryMap(module);
                break;
            }
        }
    }
    return ret;
}


bool MemoryChecker::checkMemoryAccess(S2EExecutionState *state,
                                      const ModuleDescriptor *module,
                                      uint64_t start, uint64_t size, uint8_t perms)
{
    if(!m_checkMemoryErrors)
        return true;

    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    MemoryMap *memoryMap = plgState->getModuleMemoryMap(module);
    if(!memoryMap)
        return true;

    std::stringstream err;

    do {
        if(size != uint64_t(-1) && start + size < start) {
            err << "MemoryChecker::checkMemoryAccess: "
                << "BUG: freeing region of " << (size == 0 ? "zero" : "negative")
                << " size!" << std::endl;
            break;
        }

        MemoryRange range = {start, size};
        const MemoryMap::value_type *res = memoryMap->lookup_previous(range);

        if(!res) {
            err << "MemoryChecker::checkMemoryAccess: "
                    << "BUG: memory range at " << hexval(start) << " of size " << hexval(size)
                    << " cannot be accessed by instruction "<< hexval(state->getPc()) << ": it is not mapped!" << std::endl;
            break;
        }

        if(res->first.start + res->first.size < start + size) {
            err << "MemoryChecker::checkMemoryAccess: "
                    << "BUG: memory range at " << hexval(start) << " of size " << hexval(size)
                    << " can not be accessed: it is not mapped!" << std::endl
                    << "  NOTE: closest allocated memory region: " << *res->second << std::endl;
            break;
        }

        if((perms & res->second->perms) != perms) {
            err << "MemoryChecker::checkMemoryAccess: "
                    << "BUG: memory range at " << hexval(start) << " of size " << hexval(size)
                    << " can not be accessed: insufficient permissions!" << std::endl
                    << "  NOTE: requested permissions: " << hexval(perms) << std::endl
                    << "  NOTE: closest allocated memory region: " << *res->second << std::endl;
            break;
        }
    } while(false);

    if(!err.str().empty()) {
        if(m_terminateOnErrors)
            s2e()->getExecutor()->terminateStateEarly(*state, err.str());
        else
            s2e()->getWarningsStream(state) << err.str() << std::flush;
        return false;
    }

    return true;
}

bool MemoryChecker::checkMemoryLeaks(S2EExecutionState *state,
                                     const ModuleDescriptor *module)
{
    if(!m_checkMemoryLeaks)
        return true;

    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    MemoryMap *memoryMap = plgState->getModuleMemoryMap(module);
    if(!memoryMap)
        return true;

    s2e()->getDebugStream(state) << "MemoryChecker::checkMemoryLeaks" << std::endl;

    if(memoryMap->empty())
        return true;

    std::stringstream err;

    for(MemoryMap::iterator it = memoryMap->begin(), ie = memoryMap->end();
                                                     it != ie; ++it) {
        if(!it->second->permanent) {
            if(err.str().empty()) {
                err << "MemoryChecker::checkMemoryLeaks: "
                            << "memory leaks detected!" << std::endl;
            }
            err << "  NOTE: leaked memory region: "
                    << *it->second << std::endl;
        }
    }

    if(!err.str().empty()) {
        if(m_terminateOnLeaks)
            s2e()->getExecutor()->terminateStateEarly(*state, err.str());
        else
            s2e()->getWarningsStream(state) << err.str() << std::flush;
        return false;
    }

    return true;
}

} // namespace plugins
} // namespace s2e
