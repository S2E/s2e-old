/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#ifndef S2E_PLUGINS_WINDOWSAPI_H
#define S2E_PLUGINS_WINDOWSAPI_H

#include <s2e/Plugin.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>


#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/StateManager.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>
#include <s2e/Plugins/MemoryChecker.h>

#include <map>

namespace s2e {
namespace plugins {


#define DECLARE_ENTRY_POINT(name, ...) \
    void name(S2EExecutionState* state, FunctionMonitorState *fns); \
    void name##Ret(S2EExecutionState* state, ##__VA_ARGS__)

#define DECLARE_ENTRY_POINT_CALL(name, ...) \
    void name(S2EExecutionState* state, FunctionMonitorState *fns, __VA_ARGS__)

#define DECLARE_ENTRY_POINT_RET(name, ...) \
    void name##Ret(S2EExecutionState* state, ##__VA_ARGS__)

#define DECLARE_ENTRY_POINT_CO(name, ...) \
    void name(S2EExecutionState* state, FunctionMonitorState *fns);

#define DECLARE_EP_STRUC(cl, n) { #n, &cl::n }

#define HANDLER_TRACE_CALL() s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl
#define HANDLER_TRACE_RETURN() s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << std::endl
#define HANDLER_TRACE_FCNFAILED() s2e()->getDebugStream() << "Original " << __FUNCTION__ << " failed" << std::endl
#define HANDLER_TRACE_FCNFAILED_VAL(val) s2e()->getDebugStream() << "Original " << __FUNCTION__ << " failed with 0x" << std::hex << (val) << std::endl
#define HANDLER_TRACE_PARAM_FAILED(val) s2e()->getDebugStream() << "Faild to read parameter " << val << " in " << __FUNCTION__ << " at line " << __LINE__ << std::endl

template <typename F>
struct WindowsApiHandler {
    const char *name;
    F function;
};

//Implements methods and helpers common to all kinds of
//Windows annotations.
class WindowsApi: public Plugin
{
public:
    typedef std::set<std::string> StringSet;
    typedef std::set<std::pair<uint64_t, uint64_t> > RegisteredSignals;
    typedef std::set<ModuleDescriptor, ModuleDescriptor::ModuleByLoadBase> CallingModules;

    static bool NtSuccess(S2E *s2e, S2EExecutionState *s);
    static bool NtSuccess(S2E *s2e, S2EExecutionState *s, klee::ref<klee::Expr> &eq);
    static bool NtFailure(S2E *s2e, S2EExecutionState *s, klee::ref<klee::Expr> &expr);
    static klee::ref<klee::Expr> createFailure(S2EExecutionState *state, const std::string &varName);
    klee::ref<klee::Expr> addDisjunctionToConstraints(S2EExecutionState *state, const std::string &varName,
                                                        std::vector<uint32_t> values);
    static bool ReadUnicodeString(S2EExecutionState *state, uint32_t address, std::string &s);

    static klee::ref<klee::Expr> readParameter(S2EExecutionState *s, unsigned param);
    static bool readConcreteParameter(S2EExecutionState *s, unsigned param, uint32_t *val);
    static bool writeParameter(S2EExecutionState *s, unsigned param, klee::ref<klee::Expr> val);

    enum Consistency {
        OVERCONSTR, STRICT, LOCAL, OVERAPPROX
    };

    //Maps a function name to a consistency
    typedef std::map<std::string, Consistency> ConsistencyMap;

protected:
    //XXX: should determine the base set of dependent plugins
    WindowsMonitor *m_windowsMonitor;
    ModuleExecutionDetector *m_detector;
    FunctionMonitor *m_functionMonitor;
    MemoryChecker *m_memoryChecker;
    StateManager *m_manager;

    //Allows specifying per-function consistency
    ConsistencyMap m_specificConsistency;
    Consistency m_consistency;

    RegisteredSignals m_signals;

    //The modules that will call registered API functions.
    //Annotations will not be triggered when calling from other modules.
    CallingModules m_callingModules;


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

    klee::ref<klee::Expr> forkStates(S2EExecutionState *state, std::vector<S2EExecutionState*> &result, int count,
                    const std::string &varName);

    //The current state is failed, the forked on will invoke the original API
    S2EExecutionState* forkSuccessFailure(S2EExecutionState *state, bool bypass,
                                          unsigned argCount,
                                          const std::string &varName);

    FunctionMonitor::CallSignal* getCallSignalForImport(Imports &I, const std::string &dll, const std::string &name,
                                      S2EExecutionState *state);

    //Provides a common method for configuring consistency for Windows modules
    void parseSpecificConsistency(const std::string &key);
    void parseConsistency(const std::string &key);
    Consistency getConsistency(const std::string &fcn) const;


    template <typename HANDLING_PLUGIN, typename HANDLER_PTR>
    static std::map<std::string, HANDLER_PTR> initializeHandlerMap() {
        typedef std::map<std::string, HANDLER_PTR> Map;
        Map myMap;

        unsigned elemCount = sizeof(HANDLING_PLUGIN::s_handlers) / sizeof(WindowsApiHandler<HANDLER_PTR>);
        for (unsigned i=0; i<elemCount; ++i) {
            myMap[HANDLING_PLUGIN::s_handlers[i].name] = HANDLING_PLUGIN::s_handlers[i].function;
        }
        return myMap;
    }

    //Allows to specify which imported functions should be ignored if they don't have any annotation.
    //This prevents cluttering in the log and makes it simpler to find at a glance which new implementations
    //should be annotated
    template <typename HANDLING_PLUGIN>
    static StringSet initializeIgnoredFunctionSet() {
        StringSet ignoredFunctions;
        for (unsigned i=0; HANDLING_PLUGIN::s_ignoredFunctionsList[i]; ++i) {
            ignoredFunctions.insert(std::string(HANDLING_PLUGIN::s_ignoredFunctionsList[i]));
        }
        return ignoredFunctions;
    }

    template <typename HANDLING_PLUGIN, typename HANDLER_PTR>
    bool registerEntryPoint(S2EExecutionState *state,
                            HANDLING_PLUGIN *handlingPlugin,
                            HANDLER_PTR handler, uint64_t address)
    {
        if (!address) {
            return false;
        }
        FunctionMonitor::CallSignal* cs;
        cs = m_functionMonitor->getCallSignal(state, address, 0);
        cs->connect(sigc::mem_fun(*handlingPlugin, handler));
        return true;
    }

    template <typename HANDLING_PLUGIN, typename HANDLER_PTR, typename T1>
    bool registerEntryPoint(S2EExecutionState *state,
                            HANDLING_PLUGIN *handlingPlugin,
                            HANDLER_PTR handler, uint64_t address, T1 t1)
    {
        if (!address) {
            return false;
        }
        FunctionMonitor::CallSignal* cs;
        cs = m_functionMonitor->getCallSignal(state, address, 0);
        cs->connect(sigc::bind(sigc::mem_fun(*handlingPlugin, handler), t1));
        return true;
    }

    template <class HANDLING_PLUGIN, typename HANDLER_PTR>
    static HANDLER_PTR getEntryPoint(const std::string &name) {
        typedef std::map<std::string, HANDLER_PTR> Map;
        const Map &myMap = HANDLING_PLUGIN::s_handlersMap;
        typename Map::const_iterator it = myMap.find(name);
        if (it != myMap.end()) {
            return (*it).second;
        }
        return NULL;
    }

    template <typename HANDLING_PLUGIN, typename HANDLER_PTR>
    bool registerImport(S2EExecutionState *state,
                        HANDLING_PLUGIN *handlingPlugin,
                        const std::string &importName, uint64_t address)
    {
        HANDLER_PTR handler;
        FunctionMonitor::CallSignal* cs;

        //Fetch the address of the handler
        handler = getEntryPoint<HANDLING_PLUGIN, HANDLER_PTR>(importName);
        if (!handler) {
            return false;
        }

        //Check that the import is not ignored
        if (HANDLING_PLUGIN::s_ignoredFunctions.find(importName) != HANDLING_PLUGIN::s_ignoredFunctions.end()) {
            s2e()->getWarningsStream() << "Import " << importName << " marked as ignored but has an annotation. Fix the plugin." << std::endl;
            exit(-1);
        }

        //Do not register signals multiple times
        if (m_signals.find(std::make_pair(address, 0)) != m_signals.end()) {
            return true;
        }

        cs = m_functionMonitor->getCallSignal(state, address, 0);
        cs->connect(sigc::mem_fun(*handlingPlugin, handler));
        return true;
    }

    template <typename HANDLING_PLUGIN, typename HANDLER_PTR>
    WindowsApi* registerImports(S2EExecutionState *state,
            const std::string &pluginName, const ImportedFunctions &functions)
    {
        HANDLING_PLUGIN *plugin = static_cast<HANDLING_PLUGIN*>(s2e()->getPlugin(pluginName));
        if (!plugin) {
            s2e()->getWarningsStream() << "Plugin " << pluginName << " not available" << std::endl;
            return NULL;
        }

        foreach2(it, functions.begin(), functions.end()) {
            if (!registerImport<HANDLING_PLUGIN, HANDLER_PTR>(state, plugin, (*it).first, (uint64_t)(*it).second)) {
                if (HANDLING_PLUGIN::s_ignoredFunctions.find((*it).first) == HANDLING_PLUGIN::s_ignoredFunctions.end()) {
                    s2e()->getWarningsStream() << "Import " << (*it).first << " not supported by " << pluginName << std::endl;
                }
            }
        }
        return plugin;
    }

    void registerCaller(const ModuleDescriptor &modDesc)
    {
        m_callingModules.insert(modDesc);
    }

    void unregisterCaller(const ModuleDescriptor &modDesc)
    {
        m_callingModules.erase(modDesc);
    }

    const ModuleDescriptor *calledFromModule(S2EExecutionState *s);


    void registerImports(S2EExecutionState *state, const ModuleDescriptor &module);

    ///////////////////////////////////

    const std::string getVariableName(S2EExecutionState *state, const std::string &base);
public:
    void onModuleUnload(S2EExecutionState* state, const ModuleDescriptor &module);

};

}
}

#endif
