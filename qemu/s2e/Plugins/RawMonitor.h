#ifndef _RAWMONITOR_PLUGIN_H_

#define _RAWMONITOR_PLUGIN_H_

#include <s2e/Plugins/ModuleDescriptor.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>

#include <vector>

namespace s2e {
namespace plugins {

class RawMonitor:public OSMonitor
{
    S2E_PLUGIN

public:
    struct Cfg {
        std::string name;
        uint64_t start;
        uint64_t size;
        uint64_t nativebase;
        bool delayLoad;
        bool kernelMode;
    };

    typedef std::vector<Cfg> CfgList;
private:
    CfgList m_cfg;
    sigc::connection m_onTranslateInstruction;

    uint64_t m_kernelStart;

    bool initSection(const std::string &cfgKey, const std::string &svcId);

    void onCustomInstruction(S2EExecutionState* state, uint64_t opcode);

    void loadModule(S2EExecutionState *state, const Cfg &c, bool delay);
public:
    RawMonitor(S2E* s2e): OSMonitor(s2e) {}
    virtual ~RawMonitor();
    void initialize();

    void onTranslateInstructionStart(ExecutionSignal *signal,
                                     S2EExecutionState *state,
                                     TranslationBlock *tb,
                                     uint64_t pc);

    virtual bool getImports(S2EExecutionState *s, const ModuleDescriptor &desc, Imports &I);
    virtual bool getExports(S2EExecutionState *s, const ModuleDescriptor &desc, Exports &E);
    virtual bool isKernelAddress(uint64_t pc) const;
    virtual uint64_t getPid(S2EExecutionState *s, uint64_t pc);
};



} // namespace plugins
} // namespace s2e


#endif
