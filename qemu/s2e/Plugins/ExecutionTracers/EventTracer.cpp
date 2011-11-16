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

#include "EventTracer.h"

#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <llvm/Support/TimeValue.h>

#include <iostream>
#include <sstream>

namespace s2e {
namespace plugins {

EventTracer::EventTracer(S2E* s2e): Plugin(s2e)
{

}

EventTracer::~EventTracer()
{

}

void EventTracer::initialize()
{
    //Check that the tracer is there
    m_Tracer = (ExecutionTracer*)s2e()->getPlugin("ExecutionTracer");
    assert(m_Tracer);

    m_Detector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    assert(m_Detector);

    m_TraceAll = false;
    m_TraceAllCfg = NULL;
    m_Debug = false;
}

bool EventTracer::initSections(TracerConfigEntryFactory cfgFactory)
{
    m_Debug = s2e()->getConfig()->getBool(getConfigKey() + ".enableDebug");

    std::vector<std::string> Sections;
    Sections = s2e()->getConfig()->getListKeys(getConfigKey());
    bool noErrors = true;

    foreach2(it, Sections.begin(), Sections.end()) {
        if (*it == "enableDebug") {
            continue;
        }

        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << '\n';
        std::stringstream sk;
        sk << getConfigKey() << "." << *it;

        TracerConfigEntry *cfgEntry = cfgFactory();
        assert(cfgEntry);

        if (!initBaseParameters(cfgEntry, sk.str(), *it)) {
            noErrors = false;
        }

        if (!initSection(cfgEntry, sk.str(), *it)) {
            noErrors = false;
        }

        if (!registerConfigEntry(cfgEntry)) {
            noErrors = false;
            break;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the " <<
            getConfigKey() << " sections" <<'\n';
        return false;
    }

    return true;
}

bool EventTracer::initBaseParameters(TracerConfigEntry *cfgEntry,
                        const std::string &cfgKey,
                        const std::string &entryId)
{
    bool ok;
    cfgEntry->traceAll = s2e()->getConfig()->getBool(cfgKey + ".traceAll");

    cfgEntry->moduleId = s2e()->getConfig()->getString(cfgKey + ".moduleId", "", &ok);
    if (!ok && !cfgEntry->traceAll) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".moduleId" << '\n';
        return false;
    }
    return true;
}

bool EventTracer::registerConfigEntry(TracerConfigEntry *cfgEntry)
{
    if (cfgEntry->traceAll) {
        if (m_Modules.size() > 0) {
            s2e()->getWarningsStream() <<
                    "EventTracer: There can be only one entry when tracing everything" << '\n';
            return false;
        }

        m_TraceAll = true;
        m_TraceAllCfg = cfgEntry;
        return true;
    }

    EventTracerCfgMap::iterator it = m_Modules.find(cfgEntry->moduleId);
    if (it != m_Modules.end()) {
        s2e()->getWarningsStream() <<
                "EventTracer: " << cfgEntry->moduleId << " defined multiple times" << '\n';
        return false;
    }

    m_Modules[cfgEntry->moduleId] = cfgEntry;
    return true;
}


}
}
