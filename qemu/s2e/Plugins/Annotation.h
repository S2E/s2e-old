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
        bool executeOnce;
        bool symbolicReturn;
        unsigned paramCount, keepReturnPathsCount;
        std::vector<AnnotationCfgEntry*> activateOnEntry;

        std::string callAnnotation;
        unsigned invocationCount, returnCount;

        AnnotationCfgEntry() {
            keepReturnPathsCount = 0;
            invocationCount = returnCount = 0;
            address = 0;
            paramCount = 0;
            isActive = executeOnce = symbolicReturn = false;
        }
    };


class LUAAnnotation;

class Annotation : public Plugin
{
    S2E_PLUGIN
public:
    typedef std::vector<AnnotationCfgEntry*> CfgEntries;

    Annotation(S2E* s2e): Plugin(s2e) {}
    virtual ~Annotation();
    void initialize();

private:
    FunctionMonitor *m_functionMonitor;
    ModuleExecutionDetector *m_moduleExecutionDetector;
    OSMonitor *m_osMonitor;
    StateManager *m_manager;
    CfgEntries m_entries;

    bool initSection(const std::string &entry, const std::string &cfgname);
    bool resolveDependencies(const std::string &entry, AnnotationCfgEntry *e);

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

    void invokeAnnotation(
            S2EExecutionState* state,
            FunctionMonitorState *fns,
            AnnotationCfgEntry *entry,
            bool isCall
        );

    friend class LUAAnnotation;
};

class LUAAnnotation
{
private:
    Annotation *m_plugin;
    bool m_doSkip;
    bool m_doKill;
    bool m_isReturn;
    bool m_succeed;
public:
    static const char className[];
    static Lunar<LUAAnnotation>::RegType methods[];

    LUAAnnotation(Annotation *plg);
    LUAAnnotation(lua_State *lua);
    ~LUAAnnotation();

    int setSkip(lua_State *L);
    int setKill(lua_State *L);
    int activateRule(lua_State *L);
    int isReturn(lua_State *L);
    int isCall(lua_State *L);
    int succeed(lua_State *L);

    friend class Annotation;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_FUNCSKIPPER_H
