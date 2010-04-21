#ifndef CODE_SELECTOR_PLUGIN_H

#define CODE_SELECTOR_PLUGIN_H

#include <s2e/Interceptor/ModuleDescriptor.h>
#include <s2e/Plugins/PluginInterface.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>

#include <inttypes.h>
#include <set>
#include <string>

#include "ModuleExecutionDetector.h"

namespace s2e {
namespace plugins {

class CodeSelector:public Plugin
{
    S2E_PLUGIN
private:
    typedef std::pair<uint64_t, uint64_t> Range;
    typedef std::vector<Range> Ranges;
    
    typedef std::map<ModuleExecutionDesc, uint8_t*,
    ModuleExecutionDesc> Bitmap;

    typedef std::set<std::string> ModuleSet;
    
    //Keep state accross tb translation signals
    //to put the right amount of calls to en/disablesymbexec.
    TranslationBlock *m_Tb;
    bool m_TbSymbexEnabled;
    const ModuleExecutionDesc* m_TbMod;
    sigc::connection m_TbConnection;

    ModuleExecutionDetector *m_ExecutionDetector;
    Bitmap m_Bitmap;
    ModuleSet m_ConfiguredModuleIds;

    void onModuleTransition(
        S2EExecutionState *state,
        const ModuleExecutionDesc *prevModule, 
        const ModuleExecutionDesc *currentModule
     );

    bool getRanges(const std::string &key, Ranges &R);
    bool validateRanges(const ModuleExecutionDesc &Desc, const Ranges &R) const;
    void getRanges(const ModuleExecutionDesc &Desc, 
                             Ranges &Include, Ranges &Exclude);
    uint8_t *initializeBitmap(const ModuleExecutionDesc &Desc);
    uint8_t *getBitmap(const ModuleExecutionDesc &Desc);

    bool isSymbolic(const ModuleExecutionDesc &Desc, uint64_t absolutePc);

public:
    CodeSelector(S2E* s2e);

    virtual ~CodeSelector();
    void initialize();

private:

    void onModuleTranslateBlockStart(
        ExecutionSignal *signal, 
        S2EExecutionState *state,
        const ModuleExecutionDesc*,
        TranslationBlock *tb,
        uint64_t pc);
    
    void onModuleTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleExecutionDesc*,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc);

    void onTranslateInstructionStart(
            ExecutionSignal *signal, 
            S2EExecutionState *state,
            TranslationBlock *tb,
            uint64_t pc
        );

    void disableSymbexSignal(S2EExecutionState *state, uint64_t pc);
    void enableSymbexSignal(S2EExecutionState *state, uint64_t pc);
};

}
}

#endif