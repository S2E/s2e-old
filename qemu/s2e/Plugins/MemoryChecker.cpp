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
                  "Interceptor", "ModuleExecutionDetector", "ExecutionTracer", "MemoryTracer");

namespace {
    struct MemoryRange {
        uint64_t start;
        uint64_t size;
    };

    //XXX: Should also add per-module permissions
    struct MemoryRegion {
        MemoryRange range;
        MemoryChecker::Permissions perms;
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
    MemoryMap m_memoryMap;

public:
    MemoryCheckerState() {}
    ~MemoryCheckerState() {}

    MemoryCheckerState *clone() const { return new MemoryCheckerState(*this); }
    static PluginState *factory(Plugin*, S2EExecutionState*) {
        return new MemoryCheckerState();
    }

    MemoryMap &getMemoryMap() {
        return m_memoryMap;
    }

    void setMemoryMap(const MemoryMap& memoryMap) {
        m_memoryMap = memoryMap;
    }
};

void MemoryChecker::initialize()
{
    ConfigFile *cfg = s2e()->getConfig();

    m_checkMemoryErrors = cfg->getBool(getConfigKey() + ".checkMemoryErrors");
    m_checkMemoryLeaks = cfg->getBool(getConfigKey() + ".checkMemoryLeaks");

    m_terminateOnErrors = cfg->getBool(getConfigKey() + ".terminateOnErrors");
    m_terminateOnLeaks = cfg->getBool(getConfigKey() + ".terminateOnLeaks");

    m_moduleDetector = static_cast<ModuleExecutionDetector*>(
                            s2e()->getPlugin("ModuleExecutionDetector"));
    assert(m_moduleDetector);

    m_memoryTracer = static_cast<MemoryTracer*>(
                s2e()->getPlugin("MemoryTracer"));
    assert(m_memoryTracer);

    m_executionTracer = static_cast<ExecutionTracer*>(
                s2e()->getPlugin("ExecutionTracer"));
    assert(m_executionTracer);

    m_osMonitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));
    assert(m_osMonitor);

    if(m_checkMemoryErrors) {
        m_moduleDetector->onModuleTransition.connect(
                sigc::mem_fun(*this,
                        &MemoryChecker::onModuleTransition)
                );
        s2e()->getCorePlugin()->onStateSwitch.connect(
                sigc::mem_fun(*this,
                        &MemoryChecker::onStateSwitch)
                );
    }
}


void MemoryChecker::onModuleTransition(S2EExecutionState *state,
                                       const ModuleDescriptor *prevModule,
                                       const ModuleDescriptor *nextModule)
{
    if(nextModule) {
        m_dataMemoryAccessConnection =
            s2e()->getCorePlugin()->onDataMemoryAccess.connect(
                sigc::mem_fun(*this, &MemoryChecker::onDataMemoryAccess)
            );
    } else {
        m_dataMemoryAccessConnection.disconnect();
    }
}

void MemoryChecker::onStateSwitch(S2EExecutionState *currentState,
                                  S2EExecutionState *nextState)
{

    const ModuleDescriptor *nextModule =
            m_moduleDetector->getModule(nextState, nextState->getPc());

    m_dataMemoryAccessConnection.disconnect();

    if(nextModule) {
        m_dataMemoryAccessConnection =
            s2e()->getCorePlugin()->onDataMemoryAccess.connect(
                sigc::mem_fun(*this, &MemoryChecker::onDataMemoryAccess)
            );
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

    uint64_t start = cast<klee::ConstantExpr>(virtualAddress)->getZExtValue();

    std::stringstream err;
    bool result = checkMemoryAccess(state, start,
                      klee::Expr::getMinBytesForWidth(value->getWidth()),
                      isWrite ? 2 : 1, err);

    if (!result) {
        m_memoryTracer->onDataMemoryAccess(state, virtualAddress, hostAddress, value, isWrite, isIO);
        if(m_terminateOnErrors)
            s2e()->getExecutor()->terminateStateEarly(*state, err.str());
        else
            s2e()->getWarningsStream(state) << err.str() << std::flush;
    }
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

    std::string typePrefix = type.substr(0, len-1);
    std::string patternPrefix = pattern.substr(0, len-1);
    return typePrefix.compare(patternPrefix) == 0;
}

void MemoryChecker::grantMemoryForModuleSections(
            S2EExecutionState *state,
            const ModuleDescriptor &module
        )
{
    foreach2(it, module.Sections.begin(), module.Sections.end()){
        const SectionDescriptor &sec = *it;

        MemoryChecker::Permissions perms = MemoryChecker::NONE;
        if (sec.isReadable()) {
            perms = MemoryChecker::Permissions(perms | MemoryChecker::READ);
        }
        if (sec.isWritable()) {
            perms = MemoryChecker::Permissions(perms | MemoryChecker::WRITE);
        }

        std::string sectionName = "section:";
        sectionName += sec.name;

        grantMemoryForModule(state,
                             &module,
                    sec.loadBase, sec.size,
                    perms, sectionName);
    }
}

void MemoryChecker::revokeMemoryForModuleSections(
            S2EExecutionState *state,
            const ModuleDescriptor &module
        )
{
    revokeMemoryForModule(state, &module, "section:*");
}

void MemoryChecker::revokeMemoryForModuleSections(
            S2EExecutionState *state
        )
{
    const ModuleDescriptor *module = m_moduleDetector->getModule(state, state->getPc());
    if (!module) {
        assert(false && "No module");
    }

    revokeMemoryForModule(state, module, "section:*");
}



void MemoryChecker::grantMemoryForModule(S2EExecutionState *state,
                 uint64_t start, uint64_t size,
                 Permissions perms,
                 const std::string &regionType)
{
    const ModuleDescriptor *module = m_moduleDetector->getModule(state, state->getPc());
    if (!module) {
        assert(false);
        return;
    }

    std::stringstream ss;
    ss << "module:" << module->Name << ":" << regionType;
    grantMemory(state, start, size, perms, ss.str(), false);
}

void MemoryChecker::grantMemoryForModule(S2EExecutionState *state,
                 const ModuleDescriptor *module,
                 uint64_t start, uint64_t size,
                 Permissions perms,
                 const std::string &regionType,
                 bool permanent)
{
    std::stringstream ss;
    ss << "module:" << module->Name << ":" << regionType;
    grantMemory(state, start, size, perms, ss.str(), permanent);
}

bool MemoryChecker::revokeMemoryForModule(S2EExecutionState *state,
                                 const std::string &regionTypePattern)
{
    const ModuleDescriptor *module = m_moduleDetector->getModule(state, state->getPc());
    if (!module) {
        return false;
    }

    std::stringstream ss;
    ss << "module:" << module->Name << ":" << regionTypePattern;
    return revokeMemory(state, ss.str());
}

bool MemoryChecker::revokeMemoryForModule(
        S2EExecutionState *state,
        const ModuleDescriptor *module,
        const std::string &regionTypePattern)
{
    std::stringstream ss;
    ss << "module:" << module->Name << ":" << regionTypePattern;
    return revokeMemory(state, ss.str());
}

void MemoryChecker::grantMemory(S2EExecutionState *state,
                                uint64_t start, uint64_t size, Permissions perms,
                                const std::string &regionType, uint64_t regionID,
                                bool permanent)
{
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    MemoryMap &memoryMap = plgState->getMemoryMap();

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

    /********************************************/
    /* Write a log entry about the grant event */
    unsigned traceEntrySize = 0;
    ExecutionTraceMemChecker::Flags traceFlags = ExecutionTraceMemChecker::GRANT;

    if (perms & READ) traceFlags = ExecutionTraceMemChecker::Flags(traceFlags | ExecutionTraceMemChecker::READ);
    if (perms & WRITE) traceFlags = ExecutionTraceMemChecker::Flags(traceFlags | ExecutionTraceMemChecker::WRITE);

    ExecutionTraceMemChecker::Serialized *traceEntry =
            ExecutionTraceMemChecker::serialize(&traceEntrySize, start, size,
                                                ExecutionTraceMemChecker::GRANT,
                                                regionType);

    m_executionTracer->writeData(state, traceEntry, traceEntrySize, TRACE_MEM_CHECKER);
    delete [] (uint8_t*)traceEntry;
    /********************************************/

    if(size == 0 || start + size < start) {
        s2e()->getWarningsStream(state) << "MemoryChecker::grantMemory: "
            << "detected region of " << (size == 0 ? "zero" : "negative")
            << " size!" << std::endl
            << "This probably means a bug in the OS or S2E API annotations" << std::endl;
        delete region;
        return;
    }

    const MemoryMap::value_type *res = memoryMap.lookup_previous(region->range);
    if (res && res->first.start + res->first.size > start) {
        s2e()->getWarningsStream(state) << "MemoryChecker::grantMemory: "
            << "detected overlapping ranges!" << std::endl
            << "This probably means a bug in the OS or S2E API annotations" << std::endl
            << "NOTE: requested region: " << *region << std::endl
            << "NOTE: overlapping region: " << *res->second << std::endl;
        delete region;
        return;
    }

    plgState->setMemoryMap(memoryMap.replace(std::make_pair(region->range, region)));

}

bool MemoryChecker::revokeMemory(S2EExecutionState *state,
                                 uint64_t start, uint64_t size, Permissions perms,
                                 const std::string &regionTypePattern, uint64_t regionID)
{
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    MemoryMap &memoryMap = plgState->getMemoryMap();

    MemoryRegion *region = new MemoryRegion();
    region->range.start = start;
    region->range.size = size;
    region->perms = perms;
    region->allocPC = state->getPc();
    region->type = regionTypePattern;
    region->id = regionID;

    s2e()->getDebugStream(state) << "MemoryChecker::revokeMemory("
            << *region << ")" << std::endl;


    /********************************************/
    /* Write a log entry about the revoke event */
    unsigned traceEntrySize = 0;
    ExecutionTraceMemChecker::Serialized *traceEntry =
            ExecutionTraceMemChecker::serialize(&traceEntrySize, start, size,
                                                ExecutionTraceMemChecker::REVOKE,
                                                regionTypePattern);


    m_executionTracer->writeData(state, traceEntry, traceEntrySize, TRACE_MEM_CHECKER);
    delete [] (uint8_t*)traceEntry;
    /********************************************/

    std::ostringstream err;

    do {
        if(size != uint64_t(-1) && start + size < start) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing region of " << (size == 0 ? "zero" : "negative")
                << " size!" << std::endl;
            break;
        }

        const MemoryMap::value_type *res = memoryMap.lookup_previous(region->range);
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
        plgState->setMemoryMap(memoryMap.remove(region->range));
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
                                 const std::string &regionTypePattern, uint64_t regionID)
{
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    MemoryMap &memoryMap = plgState->getMemoryMap();

    s2e()->getDebugStream(state) << "MemoryChecker::revokeMemory("
            << "pattern = '" << regionTypePattern << "', "
            << "regionID = " << hexval(regionID) << ")" << std::endl;

    bool ret = true;
    bool changed = true;
    while(changed) {
        changed = false;
        for(MemoryMap::iterator it = memoryMap.begin(), ie = memoryMap.end();
                                                        it != ie; ++it) {
            if(it->second->type.size()>0
                  && matchRegionType(regionTypePattern, it->second->type)
                  && (regionID == uint64_t(-1) || it->second->id == regionID)) {
                ret &= revokeMemory(state,
                             it->first.start, it->first.size,
                             it->second->perms, it->second->type, it->second->id);
                changed = true;
                memoryMap = plgState->getMemoryMap();
                break;
            }
        }
    }
    return ret;
}


bool MemoryChecker::revokeMemoryByPointerForModule(S2EExecutionState *state,
                                                   uint64_t pointer,
                           const std::string &regionTypePattern)
{
    const ModuleDescriptor *module = m_moduleDetector->getModule(state, state->getPc());
    if (!module) {
        assert(false);
        return false;
    }

    std::stringstream ss;
    ss << "module:" << module->Name << ":" << regionTypePattern;
    return revokeMemoryByPointer(state, pointer, ss.str());
}

bool MemoryChecker::revokeMemoryByPointerForModule(
        S2EExecutionState *state,
        const ModuleDescriptor *module,
        uint64_t pointer,
        const std::string &regionTypePattern)
{
    std::stringstream ss;
    ss << "module:" << module->Name << ":" << regionTypePattern;
    return revokeMemoryByPointer(state, pointer, ss.str());
}

bool MemoryChecker::revokeMemoryByPointer(S2EExecutionState *state, uint64_t pointer,
                           const std::string &regionTypePattern)
{
    uint64_t start, size;
    if (!findMemoryRegion(state, pointer, &start, &size)) {
        s2e()->getExecutor()->terminateStateEarly(*state, "Trying to free an unallocated region");
        return false;
    }
    if (start != pointer) {
        s2e()->getExecutor()->terminateStateEarly(*state, "Trying to free the middle of an allocated region");
        return false;
    }

    return revokeMemory(state, start, size, MemoryChecker::READWRITE, regionTypePattern);
}

bool MemoryChecker::checkMemoryAccess(S2EExecutionState *state,
                                      uint64_t start, uint64_t size, uint8_t perms,
                                      std::ostream &err)
{
    if(!m_checkMemoryErrors)
        return true;

    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    MemoryMap &memoryMap = plgState->getMemoryMap();

    bool hasError = false;

    do {
        if(size != uint64_t(-1) && start + size < start) {
            err << "MemoryChecker::checkMemoryAccess: "
                << "BUG: freeing region of " << (size == 0 ? "zero" : "negative")
                << " size!" << std::endl;
            hasError = true;
            break;
        }

        MemoryRange range = {start, size};
        const MemoryMap::value_type *res = memoryMap.lookup_previous(range);

        if(!res) {
            err << "MemoryChecker::checkMemoryAccess: "
                    << "BUG: memory range at " << hexval(start) << " of size " << hexval(size)
                    << " cannot be accessed by instruction "<< hexval(state->getPc()) << ": it is not mapped!" << std::endl;
            hasError = true;
            break;
        }

        if(res->first.start + res->first.size < start + size) {
            err << "MemoryChecker::checkMemoryAccess: "
                    << "BUG: memory range at " << hexval(start) << " of size " << hexval(size)
                    << " can not be accessed: it is not mapped!" << std::endl
                    << "  NOTE: closest allocated memory region: " << *res->second << std::endl;
            hasError = true;
            break;
        }

        if((perms & res->second->perms) != perms) {
            err << "MemoryChecker::checkMemoryAccess: "
                    << "BUG: memory range at " << hexval(start) << " of size " << hexval(size)
                    << " can not be accessed: insufficient permissions!" << std::endl
                    << "  NOTE: requested permissions: " << hexval(perms) << std::endl
                    << "  NOTE: closest allocated memory region: " << *res->second << std::endl;
            hasError = true;
            break;
        }
    } while(false);

    return !hasError;
}

bool MemoryChecker::findMemoryRegion(S2EExecutionState *state,
                                     uint64_t address,
                                     uint64_t *start, uint64_t *size) const
{
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    const MemoryMap &memoryMap = plgState->getMemoryMap();

    MemoryRange range = {address, 1};
    const MemoryMap::value_type *res = memoryMap.lookup_previous(range);
    if (!res) {
        return false;
    }

    if (start)
        *start = res->first.start;

    if (size)
        *size = res->first.size;

    return true;
}

bool MemoryChecker::checkMemoryLeaks(S2EExecutionState *state)
{
    if(!m_checkMemoryLeaks)
        return true;

    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    MemoryMap &memoryMap = plgState->getMemoryMap();

    s2e()->getDebugStream(state) << "MemoryChecker::checkMemoryLeaks" << std::endl;

    if(memoryMap.empty())
        return true;

    std::stringstream err;

    for(MemoryMap::iterator it = memoryMap.begin(), ie = memoryMap.end();
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
