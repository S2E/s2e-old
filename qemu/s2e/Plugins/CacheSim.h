#ifndef S2E_PLUGINS_CACHESIM_H
#define S2E_PLUGINS_CACHESIM_H

#include <s2e/Plugin.h>

#include <string>
#include <map>

namespace s2e {

class S2EExecutionState;

namespace plugins {

class Cache;

class CacheSim : public Plugin
{
    S2E_PLUGIN

protected:
    typedef std::map<std::string, Cache*> CachesMap;
    CachesMap m_caches;

    Cache* m_i1;
    Cache* m_d1;

    unsigned m_i1_length;
    unsigned m_d1_length;

    Cache* getCache(const std::string& name);

    void onMemoryAccess(S2EExecutionState* state, uint64_t hostAddress,
                   const uint8_t* buf, uint64_t size, bool isWrite);

public:
    CacheSim(S2E* s2e): Plugin(s2e), m_i1(0), m_d1(0),
                        m_i1_length(0), m_d1_length(0) {}
    ~CacheSim();

    void initialize();
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_CACHESIM_H
