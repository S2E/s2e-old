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

#include <s2e/Plugins/ModuleDescriptor.h>

#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/StateManager.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>
#include <s2e/Plugins/WindowsInterceptor/BlueScreenInterceptor.h>
#include <s2e/Plugins/MemoryChecker.h>
#include <s2e/Plugins/ExecutionStatisticsCollector.h>
#include <s2e/Plugins/ConsistencyModels.h>

#include <map>
#include <set>
#include <sstream>
#include <stack>

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

#define HANDLER_TRACE_CALL() s2e()->getDebugStream(state) << "Calling " << __FUNCTION__ << " at " << hexval(state->getPc()) << '\n'
#define HANDLER_TRACE_RETURN() s2e()->getDebugStream(state) << "Returning from " << __FUNCTION__ << " at " << hexval(state->getPc()) << '\n'
#define HANDLER_TRACE_FCNFAILED() s2e()->getDebugStream() << "Original " << __FUNCTION__ << " failed" << '\n'
#define HANDLER_TRACE_FCNFAILED_VAL(val) s2e()->getDebugStream() << "Original " << __FUNCTION__ << " failed with " <<  hexval(val) << '\n'
#define HANDLER_TRACE_PARAM_FAILED(val) s2e()->getDebugStream() << "Faild to read parameter " << val << " in " << __FUNCTION__ << " at line " << __LINE__ << '\n'

#define ASSERT_STRUC_SIZE(struc, expected) \
    if (sizeof(struc) != (expected)) { \
    s2e()->getDebugStream() << #struc <<  " has invalid size: " << sizeof(struc) << " instead of " << (expected) << "\n"; \
    exit(-1); \
    }

template <typename F>
struct WindowsApiHandler {
    const char *name;
    F function;
};


class WindowsApi: public Plugin
{
public:
    typedef std::set<std::string> StringSet;

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
    uint32_t getReturnAddress(S2EExecutionState *s);


    //Maps a function name to a consistency
    typedef std::map<std::string, ExecutionConsistencyModel> ConsistencyMap;


protected:
    //XXX: should determine the base set of dependent plugins
    WindowsMonitor *m_windowsMonitor;
    ModuleExecutionDetector *m_detector;
    FunctionMonitor *m_functionMonitor;
    MemoryChecker *m_memoryChecker;
    StateManager *m_manager;
    BlueScreenInterceptor *m_bsodInterceptor;
    ExecutionStatisticsCollector *m_statsCollector;
    ConsistencyModels *m_models;

    //Allows specifying per-function consistency
    ConsistencyMap m_specificConsistency;

    bool m_terminateOnWarnings;

public:
    WindowsApi(S2E *s2e) : Plugin(s2e) {
        m_detector = NULL;
        m_functionMonitor = NULL;
        m_windowsMonitor = NULL;
    }

    void initialize();

    void onModuleUnload(S2EExecutionState* state, const ModuleDescriptor &module);

protected:
    bool forkRange(S2EExecutionState *state, const std::string &msg, std::vector<uint32_t> values);

    klee::ref<klee::Expr> forkStates(S2EExecutionState *state, std::vector<S2EExecutionState*> &result, int count,
                    const std::string &varName);

    //The current state is failed, the forked on will invoke the original API
    S2EExecutionState* forkSuccessFailure(S2EExecutionState *state, bool bypass,
                                          unsigned argCount,
                                          const std::string &varName);

    //Provides a common method for configuring consistency for Windows modules
    void parseSpecificConsistency(const std::string &key);

    void registerImports(S2EExecutionState *state, const ModuleDescriptor &module);    
    virtual void unregisterEntryPoints(S2EExecutionState *state, const ModuleDescriptor &module) = 0;
    virtual void unregisterCaller(S2EExecutionState *state, const ModuleDescriptor &modDesc) = 0;

    ///////////////////////////////////

    //Retrieves a name for use by a symbolic variable
    const std::string getVariableName(S2EExecutionState *state, const std::string &base);

    ///////////////////////////////////
    bool grantAccessToUnicodeString(S2EExecutionState *state,
                                    uint64_t address, const std::string &regionType);

    bool revokeAccessToUnicodeString(S2EExecutionState *state,
                                    uint64_t address);

    ///////////////////////////////////
    void warning(S2EExecutionState *state, const std::string &msg) {
        if (m_terminateOnWarnings) {
            s2e()->getExecutor()->terminateStateEarly(*state, msg);
        } else {
            s2e()->getWarningsStream(state) << msg << "\n";
        }
    }

    ///////////////////////////////////
    void incrementFailures(S2EExecutionState *state) {
        if (m_statsCollector) {
            m_statsCollector->incrementLibCallFailures(state);
        }
    }

    void incrementSuccesses(S2EExecutionState *state) {
        if (m_statsCollector) {
            m_statsCollector->incrementLibCallSuccesses(state);
        }
    }

    void incrementEntryPoint(S2EExecutionState *state) {
        if (m_statsCollector) {
            m_statsCollector->incrementEntryPointCallForModule(state);
        }
    }
};

//Implements methods and helpers common to all kinds of
//Windows annotations.
template <class ANNOTATIONS_PLUGIN, class ANNOTATIONS_PLUGIN_STATE>
class WindowsAnnotations: public WindowsApi
{
public:
    typedef void (ANNOTATIONS_PLUGIN::*Annotation)(S2EExecutionState* state, FunctionMonitorState *fns);
    typedef const WindowsApiHandler<Annotation> AnnotationsArray;
    typedef std::map<std::string, Annotation> AnnotationsMap;
    typedef ANNOTATIONS_PLUGIN_STATE AnnotationsState;

    struct AnnotationCb0 {
        typedef void (ANNOTATIONS_PLUGIN::*Callback)(S2EExecutionState* state, FunctionMonitorState *fns);
    };

    template<class T1> struct AnnotationCb1 {
        typedef void (ANNOTATIONS_PLUGIN::*Callback)(S2EExecutionState* state, FunctionMonitorState *fns, T1 t1);
    };

public:

    WindowsAnnotations(S2E* s2e): WindowsApi(s2e) {

    }

    FunctionMonitor::CallSignal* getCallSignalForImport(Imports &I, const std::string &dll, const std::string &name,
                                      S2EExecutionState *state);


    ExecutionConsistencyModel getConsistency(S2EExecutionState *state, const std::string &fcn) const {
        ConsistencyMap::const_iterator it = m_specificConsistency.find(fcn);
        if (it != m_specificConsistency.end()) {
            return (*it).second;
        }

        return m_models->get(state);
    }


    void registerCaller(S2EExecutionState *state, const ModuleDescriptor &modDesc)
    {
        DECLARE_PLUGINSTATE(ANNOTATIONS_PLUGIN_STATE, state);
        plgState->registerCaller(modDesc);
    }

    virtual void unregisterCaller(S2EExecutionState *state, const ModuleDescriptor &modDesc)
    {
        DECLARE_PLUGINSTATE(ANNOTATIONS_PLUGIN_STATE, state);
        plgState->unregisterCaller(modDesc);
    }

    const ModuleDescriptor *calledFromModule(S2EExecutionState *state) {
        DECLARE_PLUGINSTATE(ANNOTATIONS_PLUGIN_STATE, state);
        uint32_t ra = getReturnAddress(state);
        if (!ra) {
            return NULL;
        }

        const ModuleDescriptor *mod = m_detector->getModule(state, ra);
        if (!mod) {
            return NULL;
        }

        const typename ANNOTATIONS_PLUGIN_STATE::CallingModules &cm = plgState->getCallingModules();

        return (cm.find(*mod) != cm.end()) ? mod : NULL;
    }

    //template <typename HANDLING_PLUGIN, typename HANDLER_PTR>
    static AnnotationsMap initializeHandlerMap() {
        AnnotationsMap myMap;

        unsigned elemCount = sizeof(ANNOTATIONS_PLUGIN::s_handlers) / sizeof(WindowsApiHandler<Annotation>);
        for (unsigned i=0; i<elemCount; ++i) {
            myMap[ANNOTATIONS_PLUGIN::s_handlers[i].name] = ANNOTATIONS_PLUGIN::s_handlers[i].function;
        }
        return myMap;
    }

    //Allows to specify which imported functions should be ignored if they don't have any annotation.
    //This prevents cluttering in the log and makes it simpler to find at a glance which new implementations
    //should be annotated
    static StringSet initializeIgnoredFunctionSet() {
        StringSet ignoredFunctions;
        for (unsigned i=0; ANNOTATIONS_PLUGIN::s_ignoredFunctionsList[i]; ++i) {
            ignoredFunctions.insert(std::string(ANNOTATIONS_PLUGIN::s_ignoredFunctionsList[i]));
        }
        return ignoredFunctions;
    }

    static SymbolDescriptors initializeExportedVariables() {
        SymbolDescriptors exportedVariables;
        for (unsigned i=0; ANNOTATIONS_PLUGIN::s_exportedVariablesList[i].size; ++i) {
            exportedVariables.insert(ANNOTATIONS_PLUGIN::s_exportedVariablesList[i]);
        }
        return exportedVariables;
    }

    template <typename T>
    bool registerAnnotation(S2EExecutionState *state, uint64_t address, typename T::Callback handler)
    {
        AnnotationsState *plgState = static_cast<AnnotationsState*>(getPluginState(state, &AnnotationsState::factory));
        return plgState->registerAnnotation<T>(address, m_windowsMonitor->getPid(state, address), handler);
    }

    /////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////
    //XXX: All these differ by the number of extra arguments. Should factor out
    //the common code.

    bool registerEntryPoint(S2EExecutionState *state,
                            typename AnnotationCb0::Callback handler, uint64_t address)
    {
        if (!address) {
            return false;
        }

        //Do not register signals multiple times
        if (!registerAnnotation<AnnotationCb0>(state, address, handler)) {
            return true;
        }

        FunctionMonitor::CallSignal* cs;
        cs = m_functionMonitor->getCallSignal(state, address, 0);
        cs->connect(sigc::mem_fun(*static_cast<ANNOTATIONS_PLUGIN*>(this), handler));
        return true;
    }

    template <typename T1>
    bool registerEntryPoint(S2EExecutionState *state,
                            typename AnnotationCb1<T1>::Callback handler, uint64_t address, T1 t1)
    {
        if (!address) {
            return false;
        }

        if (!registerAnnotation<AnnotationCb1<T1> >(state, address, handler)) {
            return true;
        }

        FunctionMonitor::CallSignal* cs;
        //XXX: All these zeros for cr3 must be fixed!
        cs = m_functionMonitor->getCallSignal(state, address, 0);
        cs->connect(sigc::bind(sigc::mem_fun(*static_cast<ANNOTATIONS_PLUGIN*>(this), handler), t1));
        return true;
    }

    /////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////

    static Annotation getEntryPoint(const std::string &name) {
        const AnnotationsMap &myMap = ANNOTATIONS_PLUGIN::s_handlersMap;
        typename AnnotationsMap::const_iterator it = myMap.find(name);
        if (it != myMap.end()) {
            return (*it).second;
        }
        return NULL;
    }

    bool registerEntryPoint(S2EExecutionState *state,
                        const std::string &entryPointName, uint64_t address)
    {
        Annotation handler;

        //Fetch the address of the handler
        handler = getEntryPoint(entryPointName);
        if (!handler) {
            return false;
        }

        return registerEntryPoint(state, handler, address);
    }

    void registerEntryPoints(S2EExecutionState *state,
            const ImportedFunctions &entryPoints)
    {
        foreach2(it, entryPoints.begin(), entryPoints.end()) {
            if (!registerEntryPoint(state, (*it).first, (uint64_t)(*it).second)) {
                if (ANNOTATIONS_PLUGIN::s_ignoredFunctions.find((*it).first) == ANNOTATIONS_PLUGIN::s_ignoredFunctions.end()) {
                    if (!isExportedVariable((*it).first)) {
                        s2e()->getWarningsStream() << "Import " << (*it).first << " not supported by " << getPluginInfo()->name << '\n';
                    }
                }
            }
        }
    }

    virtual void unregisterEntryPoints(S2EExecutionState *state, const ModuleDescriptor &module) {
        AnnotationsState *plgState = static_cast<AnnotationsState*>(getPluginState(state, &AnnotationsState::factory));
        plgState->unregisterAnnotations(&module);
    }

    /////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////

    //Checks whether the given name is an exported variable.
    //Useful for granting access rights to such variables.
    static bool isExportedVariable(const std::string &name, unsigned *size = NULL) {
        SymbolDescriptor desc = {name, 0};
        SymbolDescriptors::const_iterator it = ANNOTATIONS_PLUGIN::s_exportedVariables.find(desc);

        if (it == ANNOTATIONS_PLUGIN::s_exportedVariables.end()) {
            return false;
        }

        if (size) {
            *size = (*it).size;
        }
        return true;
    }

    void registerImportedVariables(
            S2EExecutionState *state,
            const ModuleDescriptor &module,
            const ImportedFunctions &symbols)
    {
        if (m_memoryChecker) {
            foreach2(fit, symbols.begin(), symbols.end()) {
                const std::string &fname = (*fit).first;
                uint64_t address = (*fit).second;
                unsigned size = 0;
                if (isExportedVariable(fname, &size)) {
                    assert(size > 0);
                    std::string impName = "import:";
                    impName += fname;

                    m_memoryChecker->grantMemoryForModule(state, &module, address, size,
                                                          MemoryChecker::READ,
                                                 impName, true);
                }
            }
        }
    }

    void unregisterImportedVariables(S2EExecutionState *state) {
        if (m_memoryChecker) {
            m_memoryChecker->revokeMemoryForModule(state, "import:*");
        }
    }

    void unregisterImportedVariables(S2EExecutionState *state, const ModuleDescriptor &module) {
        if (m_memoryChecker) {
            m_memoryChecker->revokeMemoryForModule(state, &module, "import:*");
        }
    }

    virtual void detectLeaks(S2EExecutionState *state,
                     const ModuleDescriptor &module) {
        if(m_memoryChecker) {
            unregisterImportedVariables(state, module);
            m_memoryChecker->revokeMemoryForModuleSections(state, module);
            m_memoryChecker->revokeMemoryForModule(state, &module, "stack");
            m_memoryChecker->checkMemoryLeaks(state);
            m_memoryChecker->checkResourceLeaks(state);
        }
    }

#if 0
    void detectLeaks(S2EExecutionState *state) {
        if(m_memoryChecker) {
            unregisterImportedVariables(state);
            m_memoryChecker->revokeMemoryForModuleSections(state);
            m_memoryChecker->revokeMemoryForModule(state, "stack");
            m_memoryChecker->checkMemoryLeaks(state);
        }
    }
#endif

    bool changeConsistencyForEntryPoint(
            S2EExecutionState *state,
            ExecutionConsistencyModel model,
            unsigned invocationThreshold) {

        if (m_statsCollector) {
            if (m_statsCollector->getTotalEntryPointCallCountForModule(state) > invocationThreshold) {
                m_models->push(state, model);
                return true;
            }
        }

        return false;
    }

public:

    friend class WindowsApi;
};

template <class ANNOTATIONS_PLUGIN>
class WindowsApiState: public PluginState
{
private:
    struct Annotation {
        uint64_t pc, pid;
        uint8_t handler[16];

        Annotation() {
            pc = pid = 0;
            memset(handler, 0, sizeof(handler));
        }

        bool operator()(const Annotation &a1, const Annotation &a2) const {
            if (a1.pc != a2.pc) return a1.pc < a2.pc;
            if (a1.pid != a2.pid) return a1.pid < a2.pid;
            return memcmp(a1.handler, a2.handler, sizeof(handler)) < 0;
        }
    };

public:
    typedef std::set<Annotation, Annotation> RegisteredAnnotations;
    typedef typename std::set<Annotation, Annotation>::iterator AnnotationsIterator;
    typedef std::set<ModuleDescriptor, ModuleDescriptor::ModuleByLoadBase> CallingModules;

private:
    RegisteredAnnotations m_annotations;

    //The modules that will call registered API functions.
    //Annotations will not be triggered when calling from other modules.
    CallingModules m_callingModules;


public:

    template <class T>
    bool registerAnnotation(uint64_t pc, uint64_t pid, typename T::Callback annotation) {
        assert(sizeof(annotation) <= 16);

        Annotation a;
        a.pc = pc;
        a.pid = pid;
        memcpy(a.handler, &annotation, sizeof(annotation));

        if (m_annotations.find(a) != m_annotations.end())  {
            return false;
        }
        m_annotations.insert(a);
        return true;
    }


    void unregisterAnnotations(const ModuleDescriptor *mod) {
        AnnotationsIterator it;
        it = m_annotations.begin();
        while(it != m_annotations.end()) {
            AnnotationsIterator it1 = it;
            ++it1;

            const Annotation &a = *it;
            if (mod->Contains(a.pc) && a.pid == mod->Pid) {
                m_annotations.erase(*it);
            }
            it = it1;
        }
    }

    WindowsApiState(){}

    virtual ~WindowsApiState(){};
    virtual WindowsApiState* clone() const {
        return new WindowsApiState<ANNOTATIONS_PLUGIN>(*this);
    }

    static PluginState *factory(Plugin *p, S2EExecutionState *s) {
        return new WindowsApiState();
    }


    void registerCaller(const ModuleDescriptor &modDesc) {
        m_callingModules.insert(modDesc);
    }

    void unregisterCaller(const ModuleDescriptor &modDesc) {
        m_callingModules.erase(modDesc);
    }

    const CallingModules& getCallingModules() const {
        return m_callingModules;
    }


};



}
}

#endif
