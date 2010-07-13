#ifndef S2E_PLUGINS_FUNCTIONMONITOR_H
#define S2E_PLUGINS_FUNCTIONMONITOR_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <tr1/unordered_map>

namespace s2e {
namespace plugins {

class FunctionMonitor : public Plugin
{
    S2E_PLUGIN
public:
    FunctionMonitor(S2E* s2e): Plugin(s2e) {}

    typedef sigc::signal<void, S2EExecutionState*> ReturnSignal;
    typedef sigc::signal<void, S2EExecutionState*, ReturnSignal*> CallSignal;

    /* Get a signal that is emited on function calls. Passing eip = 0 means
       any function, and cr3 = 0 means any cr3 */
    CallSignal* getCallSignal(uint64_t eip, uint64_t cr3 = 0);

    void initialize();

protected:
    void slotTranslateBlockEnd(ExecutionSignal*, S2EExecutionState *state,
                               TranslationBlock *tb, uint64_t pc,
                               bool, uint64_t);

    void slotTranslateJumpStart(ExecutionSignal *signal,
                                S2EExecutionState *state,
                                TranslationBlock*,
                                uint64_t, int jump_type);

    void slotCall(S2EExecutionState* state, uint64_t pc);
    void slotRet(S2EExecutionState* state, uint64_t pc);

    void slotTraceCall(S2EExecutionState *state, ReturnSignal *signal);
    void slotTraceRet(S2EExecutionState *state, int f);

protected:
    struct CallDescriptor {
        uint64_t cr3;
        // TODO: add sourceModuleID and targetModuleID
        CallSignal signal;
    };

    struct ReturnDescriptor {
        S2EExecutionState *state;
        uint64_t cr3;
        // TODO: add sourceModuleID and targetModuleID
        ReturnSignal signal;
    };

    typedef std::tr1::unordered_multimap<uint64_t, CallDescriptor> CallDescriptorsMap;
    typedef std::tr1::unordered_multimap<uint64_t, ReturnDescriptor> ReturnDescriptorsMap;

    CallDescriptorsMap m_callDescriptors;
    ReturnDescriptorsMap m_returnDescriptors;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_FUNCTIONMONITOR_H
