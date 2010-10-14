#ifndef S2E_PLUGINS_DEBUG_H
#define S2E_PLUGINS_DEBUG_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {

/**
 *  This is a plugin for aiding in debugging guest code.
 *  XXX: To be replaced by gdb.
 */
class Debugger : public Plugin
{
    S2E_PLUGIN
public:
    Debugger(S2E* s2e): Plugin(s2e) {}
    virtual ~Debugger();

    void initialize();

    struct AddressRange {
        AddressRange(uint64_t s, uint64_t e) {
            start = s;
            end = e;
        }

        uint64_t start, end;
    };

private:

    uint64_t *m_dataTriggers;
    unsigned m_dataTriggerCount;

    std::vector<AddressRange> m_addressTriggers;

    bool m_monitorStack;
    uint64_t m_catchAbove;

    uint64_t m_timeTrigger;
    uint64_t m_elapsedTics;
    sigc::connection m_timerConnection;

    void initList(const std::string &key, uint64_t **ptr, unsigned *size);
    void initAddressTriggers(const std::string &key);

    bool dataTriggered(uint64_t data) const;
    bool addressTriggered(uint64_t address) const;

    bool decideTracing(S2EExecutionState *state, uint64_t addr, uint64_t data) const;

    void onDataMemoryAccess(S2EExecutionState *state,
                                   klee::ref<klee::Expr> address,
                                   klee::ref<klee::Expr> hostAddress,
                                   klee::ref<klee::Expr> value,
                                   bool isWrite, bool isIO);


    void onTranslateInstructionStart(
        ExecutionSignal *signal,
        S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t pc
        );

    void onInstruction(S2EExecutionState *state, uint64_t pc);

    void onTimer();

};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
