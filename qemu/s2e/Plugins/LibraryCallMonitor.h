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

#ifndef S2E_PLUGINS_LIBCALLMON_H
#define S2E_PLUGINS_LIBCALLMON_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <string>

#include "ModuleExecutionDetector.h"
#include "FunctionMonitor.h"
#include "OSMonitor.h"

namespace s2e {
namespace plugins {

class LibraryCallMonitor : public Plugin
{
    S2E_PLUGIN
public:
    typedef std::tr1::unordered_set<std::string> StringSet;
    typedef std::set<std::pair<uint64_t, uint64_t> > AddressPairs;

    LibraryCallMonitor(S2E* s2e): Plugin(s2e) {}

    void initialize();

    sigc::signal<void,
                 S2EExecutionState*,
                 FunctionMonitorState*,
                 const ModuleDescriptor& /* The module  being called */>
          onLibraryCall;

private:
    OSMonitor * m_monitor;
    ModuleExecutionDetector *m_detector;
    FunctionMonitor *m_functionMonitor;
    StringSet m_functionNames;
    AddressPairs m_alreadyCalledFunctions;

    //List of modules whose calls we want to track.
    //Empty to track all modules in the system.
    StringSet m_trackedModules;

    bool m_displayOnce;

    void onModuleLoad(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void onModuleUnload(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );
    void onFunctionCall(S2EExecutionState* state, FunctionMonitorState *fns);
};

class LibraryCallMonitorState : public PluginState
{
public:
    typedef std::tr1::unordered_map<uint64_t, const char *> AddressToFunctionName;

private:
    AddressToFunctionName m_functions;

public:
    LibraryCallMonitorState();
    virtual ~LibraryCallMonitorState();
    virtual LibraryCallMonitorState* clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    friend class LibraryCallMonitor;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_LIBCALLMON_H
