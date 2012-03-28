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

extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}


#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>

#include "LibraryCallMonitor.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(LibraryCallMonitor, "Flags all calls to external libraries", "LibraryCallMonitor",
                  "Interceptor", "FunctionMonitor", "ModuleExecutionDetector");

void LibraryCallMonitor::initialize()
{
    m_functionMonitor = static_cast<FunctionMonitor*>(s2e()->getPlugin("FunctionMonitor"));
    m_monitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));
    m_detector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));

    ConfigFile *cfg = s2e()->getConfig();
    m_displayOnce = cfg->getBool(getConfigKey() + ".displayOnce", false);

    bool ok = false;

    //Fetch the list of modules where to report the calls
    ConfigFile::string_list moduleList =
            cfg->getStringList(getConfigKey() + ".moduleIds", ConfigFile::string_list(), &ok);

    if (!ok || moduleList.empty()) {
        s2e()->getWarningsStream() << "LibraryCallMonitor: no modules specified, tracking everything.\n";
    }

    foreach2(it, moduleList.begin(), moduleList.end()) {
        if (!m_detector->isModuleConfigured(*it)) {
            s2e()->getWarningsStream() << "LibraryCallMonitor: module " << *it
                    << " is not configured\n";
            exit(-1);
        }
        m_trackedModules.insert(*it);
    }

    m_detector->onModuleLoad.connect(
            sigc::mem_fun(*this,
                    &LibraryCallMonitor::onModuleLoad)
            );

    m_monitor->onModuleUnload.connect(
            sigc::mem_fun(*this,
                    &LibraryCallMonitor::onModuleUnload)
            );
}


void LibraryCallMonitor::onModuleLoad(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    Imports imports;

    if (!m_monitor->getImports(state, module, imports)) {
        s2e()->getWarningsStream() << "LibraryCallMonitor could not retrieve imported functions in " << module.Name << '\n';
        return;
    }

    //Unless otherwise specified, LibraryCallMonitor tracks all library calls in the system
    if (!m_trackedModules.empty()) {
        const std::string *moduleId = m_detector->getModuleId(module);
        if (!moduleId || (m_trackedModules.find(*moduleId) == m_trackedModules.end())) {
            return;
        }
    }

    DECLARE_PLUGINSTATE(LibraryCallMonitorState, state);

    foreach2(it, imports.begin(), imports.end()) {
        const std::string &libName = (*it).first;
        const ImportedFunctions &funcs = (*it).second;
        foreach2(fit, funcs.begin(), funcs.end()) {
            const std::string &funcName = (*fit).first;
            std::string composedName = libName + "!";
            composedName = composedName + funcName;

            uint64_t address = (*fit).second;

            std::pair<StringSet::iterator, bool> insertRes;
            insertRes = m_functionNames.insert(composedName);

            const char *cstring = (*insertRes.first).c_str();
            plgState->m_functions[address] = cstring;

            FunctionMonitor::CallSignal *cs = m_functionMonitor->getCallSignal(state, address, module.Pid);
            cs->connect(sigc::mem_fun(*this, &LibraryCallMonitor::onFunctionCall));
        }
    }
}

void LibraryCallMonitor::onModuleUnload(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    m_functionMonitor->disconnect(state, module);
    return;
}

void LibraryCallMonitor::onFunctionCall(S2EExecutionState* state, FunctionMonitorState *fns)
{
    //Only track configured modules
    uint64_t caller = state->getTb()->pcOfLastInstr;
    const ModuleDescriptor *mod = m_detector->getModule(state, caller);
    if (!mod) {
        return;
    }

    DECLARE_PLUGINSTATE(LibraryCallMonitorState, state);
    uint64_t pc = state->getPc();

    if (m_displayOnce && (m_alreadyCalledFunctions.find(std::make_pair(mod->Pid, pc)) != m_alreadyCalledFunctions.end())) {
        return;
    }

    LibraryCallMonitorState::AddressToFunctionName::iterator it = plgState->m_functions.find(pc);
    if (it != plgState->m_functions.end()) {
        const char *str = (*it).second;
        s2e()->getMessagesStream() << mod->Name << "@" << hexval(mod->ToNativeBase(caller)) << " called function " << str << '\n';

        onLibraryCall.emit(state, fns, *mod);

        if (m_displayOnce) {
            m_alreadyCalledFunctions.insert(std::make_pair(mod->Pid, pc));
        }
    }
}


LibraryCallMonitorState::LibraryCallMonitorState()
{

}

LibraryCallMonitorState::~LibraryCallMonitorState()
{

}

LibraryCallMonitorState* LibraryCallMonitorState::clone() const
{
    return new LibraryCallMonitorState(*this);
}

PluginState *LibraryCallMonitorState::factory(Plugin *p, S2EExecutionState *s)
{
    return new LibraryCallMonitorState();
}

} // namespace plugins
} // namespace s2e
