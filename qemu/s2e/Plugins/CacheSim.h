#ifndef S2E_PLUGINS_CACHESIM_H
#define S2E_PLUGINS_CACHESIM_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>

#include "ModuleExecutionDetector.h"

#include <klee/Expr.h>

#include <string>
#include <map>
#include <inttypes.h>

namespace s2e {

class S2EExecutionState;

namespace plugins {

class Cache;

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

    typedef std::map<std::string, Cache*> CachesMap;
    CachesMap m_caches;

    Cache* m_i1;
    Cache* m_d1;

    std::vector<CacheLogEntry> m_cacheLog;

    unsigned m_i1_length;
    unsigned m_d1_length;

    ModuleExecutionDetector *m_execDetector;
    bool m_reportZeroMisses;
    bool m_profileModulesOnly;
    sigc::connection m_ModuleConnection;

    Cache* getCache(const std::string& name);

    void flushLogEntries();
    
    void onModuleTranslateBlockStart(
        ExecutionSignal* signal, 
        S2EExecutionState *state, 
        const ModuleExecutionDesc*desc,
        TranslationBlock *tb, uint64_t pc);


    void onMemoryAccess(S2EExecutionState* state,
                        uint64_t address, unsigned size,
                        bool isWrite, bool isIO, bool isCode);

    void onDataMemoryAccess(S2EExecutionState* state,
                        klee::ref<klee::Expr> address,
                        klee::ref<klee::Expr> value,
                        bool isWrite, bool isIO);

    void onTranslateBlockStart(ExecutionSignal* signal,
                        S2EExecutionState*,
                        TranslationBlock*,
                        uint64_t);

    void onExecuteBlockStart(S2EExecutionState* state, uint64_t pc,
                             TranslationBlock* tb);

    void onTimer();

public:
    CacheSim(S2E* s2e): Plugin(s2e), m_i1(0), m_d1(0),
                        m_i1_length(0), m_d1_length(0) {}
    ~CacheSim();

    void initialize();
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_CACHESIM_H
