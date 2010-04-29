#ifndef S2E_PLUGINS_BRANCH_COVERAGE_H
#define S2E_PLUGINS_BRANCH_COVERAGE_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include "ModuleExecutionDetector.h"
#include <fstream>
#include <set>

namespace s2e {
namespace plugins {

class BranchCoverage : public Plugin
{
    S2E_PLUGIN
private:
    std::string m_FileName;
    std::ofstream m_Out;
    std::set<std::string> m_Modules;

public:
    BranchCoverage(S2E* s2e): Plugin(s2e) {}

    void initialize();
    
private:
    bool initSection(const std::string &cfgKey);
    bool initAggregatedCoverage(const std::string &cfgKey);

    void onExecution(S2EExecutionState *state, uint64_t pc);

    void onTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleExecutionDesc*,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc);
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
