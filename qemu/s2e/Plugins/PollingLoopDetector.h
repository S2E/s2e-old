#ifndef S2E_PLUGINS_POLLING_H
#define S2E_PLUGINS_POLLING_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/OSMonitor.h>

namespace s2e {
namespace plugins {

class PollingLoopDetector : public Plugin
{
    S2E_PLUGIN
public:
    PollingLoopDetector(S2E* s2e): Plugin(s2e) {}

    void initialize();

private:
    sigc::connection m_connection;


    void onModuleLoad(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void onModuleUnload(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void onModuleTranslateBlockEnd(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            const ModuleDescriptor &module,
            TranslationBlock *tb,
            uint64_t endPc,
            bool staticTarget,
            uint64_t targetPc);

    void onPollingInstruction(S2EExecutionState* state, uint64_t sourcePc);

    ModuleExecutionDetector *m_detector;
    OSMonitor *m_monitor;

};

class PollingLoopDetectorState : public PluginState
{
public:
    struct PollingEntry {
        uint64_t source;
        uint64_t dest;
        bool operator()(const PollingEntry &p1, const PollingEntry &p2) const {
            return p1.source < p2.source;
        }

        bool operator==(const PollingEntry &p1) const {
            return p1.source == source && p1.dest == dest;
        }
    };

    typedef std::set<PollingEntry, PollingEntry> PollingEntries;

private:
    PollingEntries m_pollingEntries;

public:
    PollingLoopDetectorState();
    PollingLoopDetectorState(S2EExecutionState *s, Plugin *p);
    virtual ~PollingLoopDetectorState();
    virtual PluginState *clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    void addEntry(uint64_t source, uint64_t dest);
    PollingEntries &getEntries();

    bool isPolling(uint64_t source) const;
    bool isPolling(uint64_t source, uint64_t dest) const;

    friend class PollingLoopDetector;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
