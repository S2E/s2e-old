#ifndef S2E_PLUGINS_DATA_SELECTOR_H
#define S2E_PLUGINS_DATA_SELECTOR_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>

namespace s2e {
namespace plugins {

//This class contains generic methods to parse
//the constraints specified in the configuration files.
//Subclasses should implement platform-specific selector.
class DataSelector : public Plugin
{
public:
    DataSelector(S2E* s2e): Plugin(s2e) {}
    
    void initialize();
protected:
    static klee::ref<klee::Expr> getNonNullCharacter(klee::Expr::Width w);
    static klee::ref<klee::Expr> getUpperBound(uint64_t upperBound, klee::Expr::Width w);
    static klee::ref<klee::Expr> getOddValue(klee::Expr::Width w);
    static klee::ref<klee::Expr> getOddValue(klee::Expr::Width w, uint64_t upperBound);
    bool makeUnicodeStringSymbolic(S2EExecutionState *s, uint64_t address);

    virtual bool initSection(const std::string &cfgKey, const std::string &svcId) = 0;

    ModuleExecutionDetector *m_ExecDetector;
    
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
