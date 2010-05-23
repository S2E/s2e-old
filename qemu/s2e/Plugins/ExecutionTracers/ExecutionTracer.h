#ifndef S2E_PLUGINS_EXECTRACER_H
#define S2E_PLUGINS_EXECTRACER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>
#include <s2e/S2EExecutionState.h>

#include <stdio.h>

#include "TraceEntries.h"

namespace s2e {
namespace plugins {

//Maps a module descriptor to an id, for compression purposes
typedef std::multimap<ModuleDescriptor, uint16_t, ModuleDescriptor::ModuleByLoadBase> ExecTracerModules;

/**
 *  This plugin manages the binary execution trace file.
 *  It makes sure that all the writes properly go through it.
 *  Each write is encapsulated in an ExecutionTraceItem before being
 *  written to the file.
 */
class ExecutionTracer : public Plugin
{
    S2E_PLUGIN

    FILE* m_LogFile;
    uint32_t m_CurrentIndex;
    OSMonitor *m_Monitor;
    ExecTracerModules m_Modules;

    uint16_t getCompressedId(const ModuleDescriptor *desc);

public:
    ExecutionTracer(S2E* s2e): Plugin(s2e) {}
    ~ExecutionTracer();
    void initialize();

    uint32_t writeData(
            S2EExecutionState *state,
            void *data, unsigned size, ExecTraceEntryType type);
private:


};


} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
