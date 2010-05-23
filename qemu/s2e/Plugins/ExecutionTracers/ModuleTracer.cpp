#include "ModuleTracer.h"
#include "TraceEntries.h"

#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <s2e/Plugins/OSMonitor.h>

#include <llvm/System/TimeValue.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(ModuleTracer, "Module load/unload tracer plugin",
                  "ModuleTracer"
                  "ExecutionTracer", "Interceptor");

ModuleTracer::ModuleTracer(S2E* s2e): EventTracer(s2e)
{

}

ModuleTracer::~ModuleTracer()
{

}

void ModuleTracer::initialize()
{
    m_Tracer = (ExecutionTracer*)s2e()->getPlugin("ExecutionTracer");
    assert(m_Tracer);

    OSMonitor *monitor = (OSMonitor*)s2e()->getPlugin("Interceptor");
    assert(monitor);

    monitor->onModuleLoad.connect(
            sigc::mem_fun(*this, &ModuleTracer::moduleLoadListener)
        );

    monitor->onModuleUnload.connect(
            sigc::mem_fun(*this, &ModuleTracer::moduleUnloadListener)
        );

    monitor->onProcessUnload.connect(
            sigc::mem_fun(*this, &ModuleTracer::processUnloadListener)
        );

}

bool ModuleTracer::initSection(
        TracerConfigEntry *cfgEntry,
        const std::string &cfgKey, const std::string &entryId)
{
    return true;
}

void ModuleTracer::moduleLoadListener(
    S2EExecutionState* state,
    const ModuleDescriptor &module
)
{
    DECLARE_PLUGINSTATE(ModuleTracerState, state);

    plgState->addModule(state, &module, m_Tracer);
}

void ModuleTracer::moduleUnloadListener(
    S2EExecutionState* state,
    const ModuleDescriptor &desc)
{
    DECLARE_PLUGINSTATE(ModuleTracerState, state);

    plgState->delModule(state, &desc, m_Tracer);
}

void ModuleTracer::processUnloadListener(
    S2EExecutionState* state,
    uint64_t pid)
{

    DECLARE_PLUGINSTATE(ModuleTracerState, state);

    plgState->delProcess(state, pid, m_Tracer);
}


bool ModuleTracer::getCurrentModule(S2EExecutionState *state,
                                    ModuleDescriptor *desc,
                                    uint32_t *index)
{
    DECLARE_PLUGINSTATE(ModuleTracerState, state);

    return plgState->getCurrentModule(state, desc, index);
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/


bool ModuleTracerState::addModule(S2EExecutionState *s, const ModuleDescriptor *m,
                                  ExecutionTracer *tracer)
{
    DescriptorMap::iterator it;
    it = m_Modules.find(*m);
    if (it != m_Modules.end()) {
        return false;
    }

    ExecutionTraceModuleLoad te;
    strncpy(te.name, m->Name.c_str(), sizeof(te.name));
    te.loadBase = m->LoadBase;
    te.nativeBase = m->NativeBase;
    te.size = m->Size;

    uint32_t itemIndex = tracer->writeData(s, &te, sizeof(te), TRACE_MOD_LOAD);
    if (!itemIndex) {
        return false;
    }

    m_Modules[*m] = itemIndex - 1;
    return true;
}

bool ModuleTracerState::delModule(S2EExecutionState *s, const ModuleDescriptor *m,
                                  ExecutionTracer *tracer)
{
    DescriptorMap::iterator it;

    it = m_Modules.find(*m);
    if (it == m_Modules.end()) {
        return false;
    }

    uint32_t itemIndex = (*it).second;
    m_Modules.erase(it);

    ExecutionTraceModuleUnload te;
    te.traceEntry = itemIndex;

    if (!tracer->writeData(s, &te, sizeof(te), TRACE_MOD_UNLOAD)) {
        return false;
    }

    return true;
}

bool ModuleTracerState::delProcess(S2EExecutionState *s, uint64_t pid,
                                   ExecutionTracer *tracer)
{
    DescriptorMap::iterator it, it1;

    for(it = m_Modules.begin(); it != m_Modules.end(); )
    {
        if ((*it).first.Pid == pid) {
            it1 = it;
            ++it1;
            delModule(s, &(*it).first, tracer);
            it = it1;
        }else {
            ++it;
        }
    }
    return true;
}

bool ModuleTracerState::getCurrentModule(S2EExecutionState *state,
                                         ModuleDescriptor *desc,
                                         uint32_t *index) const

{
    //XXX: make a generic cache class
    uint64_t pc = state->getPc();
    uint64_t pid = state->getPid();
    uint64_t id = state->getID();

    if (m_CachedDesc && id == m_CachedState) {
        uint64_t ModStart = m_CachedDesc->LoadBase;
        uint64_t ModSize = m_CachedDesc->Size;
        uint64_t ModPid = m_CachedDesc->Pid;

        if (pid == ModPid && pc >= ModStart && pc < ModStart + ModSize) {
            //We stayed in the same module
            *index = m_CachedTraceIndex;
            if (desc)
                *desc = *m_CachedDesc;
            return true;
        }
    }

    //Did not find module in the cache
    DescriptorMap::const_iterator it;

    ModuleDescriptor d;
    d.Pid = pid;
    d.LoadBase = pc;
    it = m_Modules.find(d);
    if (it == m_Modules.end()) {
        return false;
    }

    m_CachedDesc = &(*it).first;
    m_CachedState = id;
    m_CachedTraceIndex = (*it).second;

    *index = m_CachedTraceIndex;
    if (desc)
        *desc = *m_CachedDesc;
    return true;
}


ModuleTracerState::ModuleTracerState()
{
    m_CachedDesc = NULL;
}

ModuleTracerState::~ModuleTracerState()
{

}

ModuleTracerState* ModuleTracerState::clone() const
{
    return new ModuleTracerState(*this);
}

PluginState *ModuleTracerState::factory()
{
    return new ModuleTracerState();
}


}
}
