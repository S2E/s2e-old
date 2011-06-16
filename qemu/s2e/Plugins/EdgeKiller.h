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

#ifndef S2E_PLUGINS_POLLING_H
#define S2E_PLUGINS_POLLING_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/OSMonitor.h>

namespace s2e {
namespace plugins {

class EdgeKiller : public Plugin
{
    S2E_PLUGIN
public:
    EdgeKiller(S2E* s2e): Plugin(s2e) {}

    void initialize();

private:
    sigc::connection m_connection;


    void onModuleLoad(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void onModuleUnload(
            S2EExecutionState* state,
            const ModuleDescriptor &module
            );

    void onModuleTranslateBlockEnd(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            const ModuleDescriptor &module,
            TranslationBlock *tb,
            uint64_t endPc,
            bool staticTarget,
            uint64_t targetPc);

    void onEdge(S2EExecutionState* state, uint64_t sourcePc);

    ModuleExecutionDetector *m_detector;
    OSMonitor *m_monitor;

};

class EdgeKillerState : public PluginState
{
public:
    struct Edge {
        uint64_t source;
        uint64_t dest;
        bool operator()(const Edge &p1, const Edge &p2) const {
            return p1.source < p2.source;
        }

        bool operator==(const Edge &p1) const {
            return p1.source == source && p1.dest == dest;
        }
    };

    typedef std::set<Edge, Edge> EdgeEntries;

private:
    EdgeEntries m_edges;

public:
    EdgeKillerState();
    EdgeKillerState(S2EExecutionState *s, Plugin *p);
    virtual ~EdgeKillerState();
    virtual PluginState *clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    void addEdge(uint64_t source, uint64_t dest);
    EdgeEntries &getEntries();

    bool isEdge(uint64_t source) const;
    bool isEdge(uint64_t source, uint64_t dest) const;

    friend class EdgeKiller;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
