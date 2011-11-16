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

#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>

#include <sstream>

#include "EdgeKiller.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(EdgeKiller, "Kills states when a specified sequence of instructions has been executed", "EdgeKiller",
                  "ModuleExecutionDetector", "Interceptor");

void EdgeKiller::initialize()
{
    m_detector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    m_monitor = (OSMonitor*)s2e()->getPlugin("Interceptor");

    m_monitor->onModuleLoad.connect(
            sigc::mem_fun(*this,
                    &EdgeKiller::onModuleLoad)
            );

    m_monitor->onModuleUnload.connect(
            sigc::mem_fun(*this,
                    &EdgeKiller::onModuleUnload)
            );


}


/**
 *  Read the config section of the module
 */
void EdgeKiller::onModuleLoad(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    DECLARE_PLUGINSTATE(EdgeKillerState, state);

    ConfigFile *cfg = s2e()->getConfig();
    const std::string *id = m_detector->getModuleId(module);

    if (!id) {
        s2e()->getDebugStream() << "EdgeKiller could not figure out which "
                "module id was passed to onModuleLoad!" << '\n';
        module.Print(s2e()->getDebugStream());
        return;
    }

    std::stringstream ss;
    ss << getConfigKey() << "." << *id;


    ConfigFile::string_list pollingEntries = cfg->getListKeys(ss.str());

    if (pollingEntries.size() == 0) {
        s2e()->getWarningsStream() << "EdgeKiller did not find any configured entry for "
                "the module " << *id <<  "(" << ss.str() << ")" << '\n';
        return;
    }

    foreach2(it, pollingEntries.begin(), pollingEntries.end()) {
        std::stringstream ss1;
        ss1 << ss.str() << "." << *it;
        ConfigFile::integer_list il = cfg->getIntegerList(ss1.str());
        if (il.size() != 2) {
            s2e()->getWarningsStream() << "EdgeKiller entry " << ss1.str() <<
                    " must be of the form {sourcePc, destPc} format" << *id << '\n';
            continue;
        }

        bool ok = false;
        uint64_t source = cfg->getInt(ss1.str() + "[1]", 0, &ok);
        if (!ok) {
            s2e()->getWarningsStream() << "EdgeKiller could not read " << ss1.str() << "[0]" << '\n';
            continue;
        }

        uint64_t dest = cfg->getInt(ss1.str() + "[2]", 0, &ok);
        if (!ok) {
            s2e()->getWarningsStream() << "EdgeKiller could not read " << ss1.str() << "[1]" << '\n';
            continue;
        }

        //Convert the format to native address
        source = module.ToRuntime(source);
        dest = module.ToRuntime(dest);

        //Insert in the state
        plgState->addEdge(source, dest);
    }

    if (!m_connection.connected()) {
        m_connection = m_detector->onModuleTranslateBlockEnd.connect(
                sigc::mem_fun(*this,
                        &EdgeKiller::onModuleTranslateBlockEnd)
                );
    }


}

void EdgeKiller::onModuleUnload(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    DECLARE_PLUGINSTATE(EdgeKillerState, state);

    //Remove all the polling entries that belong to the module
    EdgeKillerState::EdgeEntries &entries =
            plgState->getEntries();

    EdgeKillerState::EdgeEntries::iterator it1, it2;

    it1 = entries.begin();
    while(it1 != entries.end()) {
        const EdgeKillerState::Edge &e = *it1;
        if (module.Contains(e.source)) {
            it2 = it1;
            ++it2;
            entries.erase(it1);
            it1 = it2;
        }else {
            ++it1;
        }
    }
}



/**
 *  Instrument only the blocks where we want to kill the state.
 */
void EdgeKiller::onModuleTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    DECLARE_PLUGINSTATE(EdgeKillerState, state);

    //If the target instruction is a kill point, connect the killer.
    if (plgState->isEdge(endPc)) {
        signal->connect(
            sigc::mem_fun(*this, &EdgeKiller::onEdge)
        );
    }
}


void EdgeKiller::onEdge(S2EExecutionState* state, uint64_t sourcePc)
{
    DECLARE_PLUGINSTATE(EdgeKillerState, state);
    if (plgState->isEdge(sourcePc, state->getPc())) {
        std::ostringstream ss;
        ss << "Edge from 0x" <<std::hex << sourcePc << " to 0x"
                << state->getPc();

        s2e()->getMessagesStream(state) << ss.str() << '\n';
        s2e()->getExecutor()->terminateStateEarly(*state, ss.str());
    }
}


///////////////////////////////////////////////////////////////////////////

EdgeKillerState::EdgeKillerState()
{

}

EdgeKillerState::EdgeKillerState(S2EExecutionState *s, Plugin *p)
{

}

EdgeKillerState::~EdgeKillerState()
{

}

PluginState *EdgeKillerState::clone() const
{
    return new EdgeKillerState(*this);
}

PluginState *EdgeKillerState::factory(Plugin *p, S2EExecutionState *s)
{
    return new EdgeKillerState();
}

EdgeKillerState::EdgeEntries &EdgeKillerState::getEntries()
{
    return m_edges;
}

void EdgeKillerState::addEdge(uint64_t source, uint64_t dest)
{
    Edge pe;
    pe.source = source;
    pe.dest = dest;
    m_edges.insert(pe);
}

bool EdgeKillerState::isEdge(uint64_t source) const
{
    Edge pe;
    pe.source = source;
    return m_edges.find(pe) != m_edges.end();
}

bool EdgeKillerState::isEdge(uint64_t source, uint64_t dest) const
{
    Edge pe;
    pe.source = source;
    pe.dest = dest;

    //For now we assume unique source in the list
    EdgeEntries::const_iterator it = m_edges.find(pe);
    if (it != m_edges.end())  {
        return pe == *it;
    }
    return false;
}

}
}
