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

#ifndef S2E_PLUGINS_MODULETRACER_H
#define S2E_PLUGINS_MODULETRACER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include "EventTracer.h"

#include <s2e/Plugins/ModuleExecutionDetector.h>


namespace s2e {
namespace plugins {

class ModuleTracer : public EventTracer
{
    S2E_PLUGIN

    ExecutionTracer *m_Tracer;

public:
    ModuleTracer(S2E* s2e);
    virtual ~ModuleTracer();
    void initialize();

#if 0
    bool getCurrentModule(S2EExecutionState *s,
                          ModuleDescriptor *desc,
                          uint32_t *index);
#endif

protected:
    virtual bool initSection(
            TracerConfigEntry *cfgEntry,
            const std::string &cfgKey, const std::string &entryId);

    void moduleLoadListener(
        S2EExecutionState* state,
        const ModuleDescriptor &module
    );

    void moduleUnloadListener(
        S2EExecutionState* state,
        const ModuleDescriptor &desc);

    void processUnloadListener(
        S2EExecutionState* state,
        uint64_t pid);



};

class ModuleTracerState: public PluginState
{
public:
    typedef std::map<ModuleDescriptor, uint32_t, ModuleDescriptor::ModuleByLoadBase> DescriptorMap;

private:
    DescriptorMap m_Modules;
    mutable const ModuleDescriptor *m_CachedDesc;
    mutable uint32_t m_CachedState;
    mutable uint32_t m_CachedTraceIndex;


public:

    bool addModule(S2EExecutionState *s, const ModuleDescriptor *m,
                   ExecutionTracer *tracer);
    bool delModule(S2EExecutionState *s, const ModuleDescriptor *m,
                   ExecutionTracer *tracer);
    bool delProcess(S2EExecutionState *s, uint64_t pid,
                    ExecutionTracer *tracer);

    bool getCurrentModule(S2EExecutionState *s,
                          ModuleDescriptor *desc,
                          uint32_t *index) const;

    ModuleTracerState();
    virtual ~ModuleTracerState();
    virtual ModuleTracerState* clone() const;
    static PluginState *factory();

    friend class ModuleTracer;

};


}
}
#endif
