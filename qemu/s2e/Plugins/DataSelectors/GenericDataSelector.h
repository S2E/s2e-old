#ifndef S2E_PLUGINS_GENDATASEL_H
#define S2E_PLUGINS_GENDATASEL_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include "DataSelector.h"
#include <s2e/Plugins/OSMonitor.h>

namespace s2e {
namespace plugins {

struct RuleCfg
{
    std::string moduleId;
    std::string rule;
    uint64_t pc;
};

class GenericDataSelector : public DataSelector
{
    S2E_PLUGIN
public:
    GenericDataSelector(S2E* s2e): DataSelector(s2e) {}

    void initialize();
    void onTranslateBlockStart(ExecutionSignal*, S2EExecutionState *state, 
        const ModuleExecutionDesc*desc,
        TranslationBlock *tb, uint64_t pc);
    
    void onExecution(S2EExecutionState *state, uint64_t pc,
                                unsigned idx);

private:
    //Decide where and what to inject
    std::vector<RuleCfg> m_Rules;

    OSMonitor *m_Monitor;
    sigc::connection m_TbConnection;

    virtual bool initSection(const std::string &cfgKey, const std::string &svcId);
    void injectRsaGenKey(S2EExecutionState *state);  
    
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
