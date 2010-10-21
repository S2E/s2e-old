#ifndef S2E_PLUGINS_FUNCSKIPPER_H
#define S2E_PLUGINS_FUNCSKIPPER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/OSMonitor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Plugins/StateManager.h>

namespace s2e {
namespace plugins {

    struct AnnotationCfgEntry
    {
        std::string cfgname;
        std::string module;
        uint64_t address;
        bool isActive;
        unsigned paramCount;

        bool isCallAnnotation;
        std::string annotation;
        unsigned invocationCount, returnCount;

        AnnotationCfgEntry() {
            isCallAnnotation = true;
            address = 0;
            paramCount = 0;
            isActive = false;
        }

        bool operator()(const AnnotationCfgEntry *a1, const AnnotationCfgEntry *a2) const {
            if (a1->isCallAnnotation != a2->isCallAnnotation) {
                return a1->isCallAnnotation < a2->isCallAnnotation;
            }

            int res = a1->module.compare(a2->module);
            if (!res) {
                return a1->address < a2->address;
            }
            return res < 0;
        }
    };


class LUAAnnotation;

class Annotation : public Plugin
{
    S2E_PLUGIN
public:
    typedef std::set<AnnotationCfgEntry*, AnnotationCfgEntry> CfgEntries;

    Annotation(S2E* s2e): Plugin(s2e) {}
    virtual ~Annotation();
    void initialize();

private:
    FunctionMonitor *m_functionMonitor;
    ModuleExecutionDetector *m_moduleExecutionDetector;
    OSMonitor *m_osMonitor;
    StateManager *m_manager;
    CfgEntries m_entries;

    //To instrument specific instructions in the code
    bool m_translationEventConnected;
    TranslationBlock *m_tb;
    sigc::connection m_tbConnection;


    bool initSection(const std::string &entry, const std::string &cfgname);

    void onModuleLoad(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void onFunctionRet(
            S2EExecutionState* state,
            AnnotationCfgEntry *entry
            );

    void onFunctionCall(
            S2EExecutionState* state,
            FunctionMonitorState *fns,
            AnnotationCfgEntry *entry
            );

    void onTranslateBlockStart(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            const ModuleDescriptor &module,
            TranslationBlock *tb,
            uint64_t pc);

    void onTranslateInstructionEnd(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            TranslationBlock *tb,
            uint64_t pc);

    void onModuleTranslateBlockEnd(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            const ModuleDescriptor &module,
            TranslationBlock *tb,
            uint64_t endPc,
            bool staticTarget,
            uint64_t targetPc);

    void onInstruction(S2EExecutionState *state, uint64_t pc);

    void invokeAnnotation(
            S2EExecutionState* state,
            FunctionMonitorState *fns,
            AnnotationCfgEntry *entry,
            bool isCall, bool isInstruction
        );

    friend class LUAAnnotation;
};

class AnnotationState: public PluginState
{
public:
    typedef std::map<std::string, uint64_t> Storage;

private:
    Storage m_storage;

public:
    AnnotationState();
    virtual ~AnnotationState();
    virtual AnnotationState* clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    friend class Annotation;

    uint64_t getValue(const std::string &key);
    void setValue(const std::string &key, uint64_t value);   
};

class LUAAnnotation
{
private:
    Annotation *m_plugin;
    bool m_doSkip;
    bool m_doKill;
    bool m_isReturn;
    bool m_succeed;
    bool m_isInstruction;
    S2EExecutionState *m_state;

public:
    static const char className[];
    static Lunar<LUAAnnotation>::RegType methods[];

    LUAAnnotation(Annotation *plg, S2EExecutionState *state);
    LUAAnnotation(lua_State *lua);
    ~LUAAnnotation();

    int setSkip(lua_State *L);
    int setKill(lua_State *L);
    int activateRule(lua_State *L);
    int isReturn(lua_State *L);
    int isCall(lua_State *L);
    int succeed(lua_State *L);

    int setValue(lua_State *L);
    int getValue(lua_State *L);

    friend class Annotation;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_FUNCSKIPPER_H
