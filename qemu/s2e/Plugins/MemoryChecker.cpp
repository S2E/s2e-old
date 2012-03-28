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

    //Used to track OS resources that are accessed
    //through a handle
    struct ResourceHandle {
        uint64_t allocPC;
        std::string type;
        uint64_t handle;
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

    typedef klee::ImmutableMap<uint64_t, ResourceHandle> ResourceHandleMap;

    llvm::raw_ostream& operator <<(llvm::raw_ostream& out, const MemoryRegion& r) {
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

    llvm::raw_ostream& operator <<(llvm::raw_ostream& out, const ResourceHandle& r) {
        out << "ResourceHandle(\n"
            << "    handle = " << hexval(r.handle) << "\n"
            << "    allocPC = " << hexval(r.allocPC) << "\n"
            << "    type = " << r.type << "\n";
        return out;
    }

    std::ostream& operator <<(std::ostream& out, const ResourceHandle& r) {
        out << "ResourceHandle(\n"
            << "    handle = " << hexval(r.handle) << "\n"
            << "    allocPC = " << hexval(r.allocPC) << "\n"
            << "    type = " << r.type << "\n";
        return out;
    }
} // namespace

class MemoryCheckerState: public PluginState
{
public:
    MemoryMap m_memoryMap;
    ResourceHandleMap m_resourceMap;

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

    ResourceHandleMap &getResourceMap() {
        return m_resourceMap;
    }

    void setResourceMap(const ResourceHandleMap& resourceMap) {
        m_resourceMap = resourceMap;
    }
};

void MemoryChecker::initialize()
{
    ConfigFile *cfg = s2e()->getConfig();

    m_checkMemoryErrors = cfg->getBool(getConfigKey() + ".checkMemoryErrors", true);
    m_checkMemoryLeaks = cfg->getBool(getConfigKey() + ".checkMemoryLeaks", true);
    m_checkResourceLeaks = cfg->getBool(getConfigKey() + ".checkResourceLeaks", true);

    m_terminateOnErrors = cfg->getBool(getConfigKey() + ".terminateOnErrors", true);
    m_terminateOnLeaks = cfg->getBool(getConfigKey() + ".terminateOnLeaks", true);

    m_traceMemoryAccesses = cfg->getBool(getConfigKey() + ".traceMemoryAccesses", false);

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

        s2e()->getCorePlugin()->onException.connect(
                sigc::mem_fun(*this,
                        &MemoryChecker::onException)
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

void MemoryChecker::onException(S2EExecutionState *state, unsigned intNb, uint64_t pc)
{
    //Reconnection will be done automatically upon next
    //module transition signal.
    m_dataMemoryAccessConnection.disconnect();
}

void MemoryChecker::onDataMemoryAccess(S2EExecutionState *state,
                                       klee::ref<klee::Expr> virtualAddress,
                                       klee::ref<klee::Expr> hostAddress,
                                       klee::ref<klee::Expr> value,
                                       bool isWrite, bool isIO)
{
    if (state->isRunningExceptionEmulationCode()) {
        //We do not check what memory the CPU accesses.
        //s2e()->getWarningsStream() << "Running emulation code" << std::endl;
        return;
    }

    if(!isa<klee::ConstantExpr>(virtualAddress)) {
        s2e()->getWarningsStream(state) << "Symbolic memory accesses are "
                << "not yet supported by MemoryChecker" << '\n';
        return;
    }

    //XXX: This is a hack.
    //Sometimes the onModuleTransition is not fired properly...
    if (!m_moduleDetector->getCurrentDescriptor(state)) {
        m_dataMemoryAccessConnection.disconnect();
        return;
    }

    if (m_traceMemoryAccesses) {
        m_memoryTracer->traceDataMemoryAccess(state, virtualAddress, hostAddress, value, isWrite, isIO);
    }

    uint64_t start = cast<klee::ConstantExpr>(virtualAddress)->getZExtValue();
    unsigned accessSize = klee::Expr::getMinBytesForWidth(value->getWidth());

    onPreCheck.emit(state, start, accessSize, isWrite);

    std::string errstr;
    llvm::raw_string_ostream err(errstr);
    bool result = checkMemoryAccess(state, start,
                      accessSize,
                      isWrite ? 2 : 1, err);


    if (!result) {
        onPostCheck.emit(state, start, accessSize, isWrite, &result);
        if (result) {
            return;
        }

        if(m_terminateOnErrors)
            s2e()->getExecutor()->terminateStateEarly(*state, err.str());
        else
            s2e()->getWarningsStream(state) << err.str();
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

//    s2e()->getDebugStream() << "matchRegionType typePrefix=" << typePrefix
//            << " patternPrefix=" << patternPrefix << std::endl;

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

void MemoryChecker::revokeMemoryForModuleSection(
            S2EExecutionState *state,
            const ModuleDescriptor &module,
            const std::string &section
        )
{
    revokeMemoryForModule(state, &module, "section:" + section);
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

std::string MemoryChecker::getRegionTypePrefix(S2EExecutionState *state, const std::string &regionType)
{
    const ModuleDescriptor *module = m_moduleDetector->getModule(state, state->getPc());
    if (!module) {
        assert(false);
        return "";
    }

    std::stringstream ss;
    ss << "module:" << module->Name << ":" << regionType;
    return ss.str();
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
        assert(false && "No module found");
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
            << *region << ")" << '\n';

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
            << " size!" << '\n'
            << "This probably means a bug in the OS or S2E API annotations" << '\n';
        delete region;
        return;
    }

    const MemoryMap::value_type *res = memoryMap.lookup_previous(region->range);
    if (res && res->first.start + res->first.size > start) {
        s2e()->getWarningsStream(state) << "MemoryChecker::grantMemory: "
            << "detected overlapping ranges!" << '\n'
            << "This probably means a bug in the OS or S2E API annotations" << '\n'
            << "NOTE: requested region: " << *region << '\n'
            << "NOTE: overlapping region: " << *res->second << '\n';
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
            << *region << ")" << '\n';


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

    std::string errstr;
    llvm::raw_string_ostream err(errstr);

    do {
        if(size != uint64_t(-1) && start + size < start) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing region of " << (size == 0 ? "zero" : "negative")
                << " size!" << '\n';
            break;
        }

        const MemoryMap::value_type *res = memoryMap.lookup_previous(region->range);
        if(!res || res->first.start + res->first.size <= start) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory that was not allocated!" << '\n';
            break;
        }

        if(res->first.start != start) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory that was not allocated!" << '\n'
                << "  NOTE: overlapping region exists: " << *res->second << '\n'
                << "  NOTE: requested region: " << *region << '\n';
            break;
        }

        if(size != uint64_t(-1) && res->first.size != size) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory region of wrong size!" << '\n'
                << "  NOTE: allocated region: " << *res->second << '\n'
                << "  NOTE: requested region: " << *region << '\n';
        }

        if(perms != ANY && res->second->perms != perms) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory region with wrong permissions!" << '\n'
                << "  NOTE: allocated region: " << *res->second << '\n'
                << "  NOTE: requested region: " << *region << '\n';
        }

        if(regionTypePattern.size()>0 && !matchRegionType(regionTypePattern, res->second->type)) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory region with wrong region type!" << '\n'
                << "  NOTE: allocated region: " << *res->second << '\n'
                << "  NOTE: requested region: " << *region << '\n';
        }

        if(regionID != uint64_t(-1) && res->second->id != regionID) {
            err << "MemoryChecker::revokeMemory: "
                << "BUG: freeing memory region with wrong region ID!" << '\n'
                << "  NOTE: allocated region: " << *res->second << '\n'
                << "  NOTE: requested region: " << *region << '\n';
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
            s2e()->getWarningsStream(state) << err.str();
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
            << "regionID = " << hexval(regionID) << ")" << '\n';

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

    return revokeMemory(state, start, size, ANY, regionTypePattern);
}


void MemoryChecker::grantResourceForModule(S2EExecutionState *state,
                                           uint64_t handle, const std::string &resourceType)
{
    const ModuleDescriptor *module = m_moduleDetector->getModule(state, state->getPc());
    if (!module) {
        assert(false);
        return;
    }

    std::stringstream ss;
    ss << "module:" << module->Name << ":resource:" << resourceType;
    grantResource(state, handle, resourceType);
}

void MemoryChecker::grantResource(S2EExecutionState *state,
                   uint64_t handle, const std::string &resourceType)
{
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    ResourceHandleMap &resourceMap = plgState->getResourceMap();

    ResourceHandle res;
    res.allocPC = state->getPc();
    res.handle = handle;
    res.type = resourceType;

    s2e()->getDebugStream(state) << "MemoryChecker::grantResource(" << res << ")\n";

    /********************************************/
    /* Write a log entry about the grant event */
    unsigned traceEntrySize = 0;
    ExecutionTraceMemChecker::Flags traceFlags =
            ExecutionTraceMemChecker::Flags(
            ExecutionTraceMemChecker::GRANT |
            ExecutionTraceMemChecker::RESOURCE);

    ExecutionTraceMemChecker::Serialized *traceEntry =
            ExecutionTraceMemChecker::serialize(&traceEntrySize, handle, 0,
                                                traceFlags,
                                                resourceType);

    m_executionTracer->writeData(state, traceEntry, traceEntrySize, TRACE_MEM_CHECKER);
    delete [] (uint8_t*)traceEntry;
    /********************************************/

    const ResourceHandleMap::value_type *exres = resourceMap.lookup(handle);
    if (exres) {
        s2e()->getWarningsStream(state) << "MemoryChecker::grantResource: "
            << "resource already allocated!" << '\n'
            << "This probably means a bug in the OS or S2E API annotations"<< '\n'
            << "NOTE: requested resource: " << hexval(handle) << '\n'
            << "NOTE: existing region   : " << exres->second << '\n';
        return;
    }

    plgState->setResourceMap(resourceMap.replace(std::make_pair(handle, res)));

}

void MemoryChecker::revokeResource(S2EExecutionState *state,  uint64_t handle)
{
    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    ResourceHandleMap &resourceMap = plgState->getResourceMap();

    s2e()->getDebugStream(state) << "MemoryChecker::revokeResource("
                                 << "handle = '" << hexval(handle) << ")" << '\n';

    plgState->setResourceMap(resourceMap.remove(handle));
}

bool MemoryChecker::checkMemoryAccess(S2EExecutionState *state,
                                      uint64_t start, uint64_t size, uint8_t perms,
                                      llvm::raw_ostream &err)
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
                << " size!" << '\n';
            hasError = true;
            break;
        }

        MemoryRange range = {start, size};
        const MemoryMap::value_type *res = memoryMap.lookup_previous(range);

        if(!res) {


            err << "MemoryChecker::checkMemoryAccess: "
                    << "BUG: memory range at " << hexval(start) << " of size " << hexval(size)
                    << " cannot be accessed by instruction " << getPrettyCodeLocation(state)
                    << ": it is not mapped!" << '\n';
            hasError = true;
            break;
        }

        if(res->first.start + res->first.size < start + size) {
            err << "MemoryChecker::checkMemoryAccess: "
                    << "BUG: memory range at " << hexval(start) << " of size " << hexval(size)
                    << " can not be accessed by instruction " << getPrettyCodeLocation(state)
                    << ": it is not mapped!" << '\n'
                    << "  NOTE: closest allocated memory region: " << *res->second << '\n';
            hasError = true;
            break;
        }

        if((perms & res->second->perms) != perms) {
            err << "MemoryChecker::checkMemoryAccess: "
                    << "BUG: memory range at " << hexval(start) << " of size " << hexval(size)
                    << " can not be accessed by instruction " << getPrettyCodeLocation(state)
                    << ": insufficient permissions!" << '\n'
                    << "  NOTE: requested permissions: " << hexval(perms) << '\n'
                    << "  NOTE: closest allocated memory region: " << *res->second << '\n';
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

bool MemoryChecker::checkResourceLeaks(S2EExecutionState *state)
{
    if(!m_checkResourceLeaks)
        return true;

    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    ResourceHandleMap &resourceMap = plgState->getResourceMap();

    s2e()->getDebugStream(state) << "MemoryChecker::checkResourceLeaks" << '\n';

    if (resourceMap.empty()) {
        return true;
    }

    std::stringstream err;
    ResourceHandleMap::iterator it = resourceMap.begin();

    while(it != resourceMap.end()) {
            if(err.str().empty()) {
                err << "MemoryChecker::checkResourceLeaks: "
                    << "resource leaks detected!" << '\n';
            }
            err << "  NOTE: leaked memory region: "
                << it->second << '\n';
            ++it;
    }

    if(!err.str().empty()) {
        if(m_terminateOnLeaks)
            s2e()->getExecutor()->terminateStateEarly(*state, err.str());
        else
            s2e()->getWarningsStream(state) << err.str();
        return false;
    }

    return true;
}

bool MemoryChecker::checkMemoryLeaks(S2EExecutionState *state)
{
    if(!m_checkMemoryLeaks)
        return true;

    DECLARE_PLUGINSTATE(MemoryCheckerState, state);

    MemoryMap &memoryMap = plgState->getMemoryMap();

    s2e()->getDebugStream(state) << "MemoryChecker::checkMemoryLeaks" << '\n';

    if(memoryMap.empty())
        return true;

    std::string errstr;
    llvm::raw_string_ostream err(errstr);

    for(MemoryMap::iterator it = memoryMap.begin(), ie = memoryMap.end();
                                                     it != ie; ++it) {
        if(!it->second->permanent) {
            if(err.str().empty()) {
                err << "MemoryChecker::checkMemoryLeaks: "
                            << "memory leaks detected!" << '\n';
            }
            err << "  NOTE: leaked memory region: "
                    << *it->second << '\n';
        }
    }

    if(!err.str().empty()) {
        if(m_terminateOnLeaks)
            s2e()->getExecutor()->terminateStateEarly(*state, err.str());
        else
            s2e()->getWarningsStream(state) << err.str();
        return false;
    }

    return true;
}

std::string MemoryChecker::getPrettyCodeLocation(S2EExecutionState *state)
{
    std::stringstream ss;
    if (m_moduleDetector) {
        const ModuleDescriptor *desc = m_moduleDetector->getCurrentDescriptor(state);
        if (desc) {
            uint64_t relPc = desc->ToNativeBase(state->getPc());
            ss << desc->Name << "!0x" << std::hex << relPc;
            return ss.str();
        }
    }

    ss << "<unknown module>!0x" << std::hex << state->getPc();

    return ss.str();
}

} // namespace plugins
} // namespace s2e
