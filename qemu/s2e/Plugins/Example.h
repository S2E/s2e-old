#ifndef S2E_PLUGINS_EXAMPLE_H
#define S2E_PLUGINS_EXAMPLE_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>

namespace s2e {
namespace plugins {

class Example : public Plugin, public sigc::trackable
{
    S2E_PLUGIN
public:
    Example(S2E* s2e): Plugin(s2e) {}

    void initialize();
    void slotTranslateBlockStart(ExecutionSignal*, uint64_t pc);
    void slotExecuteBlockStart(uint64_t pc);

private:
    bool m_traceBlockTranslation;
    bool m_traceBlockExecution;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
