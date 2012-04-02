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

#ifndef S2E_PLUGINS_CACHESIM_H
#define S2E_PLUGINS_CACHESIM_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>

#include "ModuleExecutionDetector.h"

#include <s2e/Plugins/ExecutionTracers/ExecutionTracer.h>

#include <klee/Expr.h>

#include <string>
#include <map>
#include <inttypes.h>

namespace s2e {

class S2EExecutionState;

namespace plugins {

class Cache;
class CacheSimState;

class CacheSim : public Plugin
{
    S2E_PLUGIN
protected:

    struct CacheLogEntry
    {
        uint64_t timestamp;
        uint64_t pc;
        uint64_t address;
        unsigned size;
        bool     isWrite;
        bool     isCode;
        const char* cacheName;
        uint32_t missCount;
    };


    std::vector<CacheLogEntry> m_cacheLog;


    ModuleExecutionDetector *m_execDetector;
    ExecutionTracer *m_Tracer;

    bool m_reportWholeSystem;
    bool m_reportZeroMisses;
    bool m_profileModulesOnly;
    bool m_cacheStructureWrittenToLog;
    bool m_startOnModuleLoad;
    bool m_physAddress;
    sigc::connection m_ModuleConnection;

    sigc::connection m_d1_connection;
    sigc::connection m_i1_connection;

    void onModuleTranslateBlockStart(
        ExecutionSignal* signal,
        S2EExecutionState *state,
        const ModuleDescriptor &desc,
        TranslationBlock *tb, uint64_t pc);


    void onMemoryAccess(S2EExecutionState* state,
                        uint64_t address, unsigned size,
                        bool isWrite, bool isIO, bool isCode);

    void onDataMemoryAccess(S2EExecutionState* state,
                        klee::ref<klee::Expr> address,
                        klee::ref<klee::Expr> hostAddress,
                        klee::ref<klee::Expr> value,
                        bool isWrite, bool isIO);

    void onTranslateBlockStart(ExecutionSignal* signal,
                        S2EExecutionState*,
                        TranslationBlock*,
                        uint64_t);

    void onExecuteBlockStart(S2EExecutionState* state, uint64_t pc,
                             TranslationBlock* tb, uint64_t hostAddress);


    void writeCacheDescriptionToLog(S2EExecutionState *state);

    bool profileAccess(S2EExecutionState *state) const;
    bool reportAccess(S2EExecutionState *state) const;
public:
    CacheSim(S2E* s2e): Plugin(s2e) {}
    ~CacheSim();

    void initialize();

    friend class CacheSimState;
};

class CacheSimState: public PluginState
{
private:
    typedef std::map<std::string, Cache*> CachesMap;
    CachesMap m_caches;

    unsigned m_i1_length;
    unsigned m_d1_length;

    Cache* m_i1;
    Cache* m_d1;

public:
    CacheSimState();
    CacheSimState(S2EExecutionState *s, Plugin *p);
    virtual ~CacheSimState();
    virtual PluginState *clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    Cache* getCache(const std::string& name);

    friend class CacheSim;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_CACHESIM_H
