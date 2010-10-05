#ifndef S2E_PLUGINS_WINDOWSAPI_H
#define S2E_PLUGINS_WINDOWSAPI_H

#include <s2e/Plugin.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>


#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>


namespace s2e {
namespace plugins {

#define REGISTER_ENTRY_POINT(cs, pc, name) \
    if (pc) {\
        s2e()->getMessagesStream() << "Registering " # name <<  " at 0x" << std::hex << pc << std::endl; \
        cs = m_functionMonitor->getCallSignal(state, pc, 0); \
        cs->connect(sigc::mem_fun(*this, &CURRENT_CLASS::name)); \
    }

#define DECLARE_ENTRY_POINT(name) \
    void name(S2EExecutionState* state, FunctionMonitorState *fns); \
    void name##Ret(S2EExecutionState* state)


#define REGISTER_IMPORT(I, dll, name) { \
    FunctionMonitor::CallSignal *__cs; \
    __cs = getCallSignalForImport(I, dll, #name, state); \
    if (__cs) __cs->connect(sigc::mem_fun(*this,  &CURRENT_CLASS::name)); \
   }

//Implements methods and helpers common to all kinds of
//Windows annotations.
class WindowsApi: public Plugin
{
public:
    typedef std::set<std::string> StringSet;

    static bool NtSuccess(S2E *s2e, S2EExecutionState *s, klee::ref<klee::Expr> &eq);
    static bool NtFailure(S2E *s2e, S2EExecutionState *s, klee::ref<klee::Expr> &expr);

    static bool ReadUnicodeString(S2EExecutionState *state, uint32_t address, std::string &s);

    static klee::ref<klee::Expr> readParameter(S2EExecutionState *s, unsigned param);
    static bool readConcreteParameter(S2EExecutionState *s, unsigned param, uint32_t *val);
    static bool writeParameter(S2EExecutionState *s, unsigned param, klee::ref<klee::Expr> val);

    enum Consistency {
        STRICT, LOCAL, OVERAPPROX, OVERCONSTR
    };

    //Maps a function name to a consistency
    typedef std::map<std::string, Consistency> ConsistencyMap;

protected:
    //XXX: should determine the base set of dependent plugins
    WindowsMonitor *m_windowsMonitor;
    ModuleExecutionDetector *m_detector;
    FunctionMonitor *m_functionMonitor;

    //Allows specifying per-function consistency
    ConsistencyMap m_specificConsistency;
    Consistency m_consistency;


    WindowsApi(S2E* s2e): Plugin(s2e) {
        m_detector = NULL;
        m_functionMonitor = NULL;
        m_windowsMonitor = NULL;
        m_consistency = STRICT;
    }

    void initialize();

    void TraceCall(S2EExecutionState* state, FunctionMonitorState *fns);
    void TraceRet(S2EExecutionState* state);

    bool forkRange(S2EExecutionState *state, const std::string &msg, std::vector<uint32_t> values);
    void forkStates(S2EExecutionState *state, std::vector<S2EExecutionState*> &result, int count);


    FunctionMonitor::CallSignal* getCallSignalForImport(Imports &I, const std::string &dll, const std::string &name,
                                      S2EExecutionState *state);

    //Provides a common method for configuring consistency for Windows modules
    void parseSpecificConsistency(const std::string &key);
    void parseConsistency(const std::string &key);
    Consistency getConsistency(const std::string &fcn) const;
};

}
}

#endif
