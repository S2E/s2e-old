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

#define DECLARE_ENTRY_POINT(name, ...) \
    void name(S2EExecutionState* state, FunctionMonitorState *fns); \
    void name##Ret(S2EExecutionState* state, ##__VA_ARGS__)

#define DECLARE_ENTRY_POINT_CO(name, ...) \
    void name(S2EExecutionState* state, FunctionMonitorState *fns);

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
