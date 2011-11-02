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

#ifndef S2E_PLUGINS_EVENTTRACER_H
#define S2E_PLUGINS_EVENTTRACER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include "ExecutionTracer.h"

#include <s2e/Plugins/ModuleExecutionDetector.h>

#include <stdio.h>
#include <map>
#include <string>

namespace s2e {
namespace plugins {


struct TracerConfigEntry {
    std::string moduleId;
    bool traceAll;

    TracerConfigEntry() {
        traceAll = false;
    }

    virtual ~TracerConfigEntry() {

    }

};

typedef TracerConfigEntry* (*TracerConfigEntryFactory)();

//Maps a module name to a configuration entry
typedef std::map<std::string, TracerConfigEntry*> EventTracerCfgMap;


/**
 *  Base class for all types of tracers.
 *  Handles the basic boilerplate (e.g., common config options).
 */
class EventTracer : public Plugin
{

protected:
    ModuleExecutionDetector *m_Detector;
    ExecutionTracer *m_Tracer;
    EventTracerCfgMap m_Modules;
    bool m_TraceAll;
    TracerConfigEntry *m_TraceAllCfg;
    bool m_Debug;

    EventTracer(S2E* s2e);
    virtual ~EventTracer();

    void initialize();

private:
    bool initBaseParameters(TracerConfigEntry *cfgEntry,
                            const std::string &cfgKey,
                            const std::string &entryId);

    bool registerConfigEntry(TracerConfigEntry *cfgEntry);

protected:
   bool initSections(TracerConfigEntryFactory cfgFactory);

   virtual bool initSection(
           TracerConfigEntry *cfgEntry,
           const std::string &cfgKey,
           const std::string &entryId) = 0;


};



}
}

#endif
