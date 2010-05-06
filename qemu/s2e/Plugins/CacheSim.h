#ifndef S2E_PLUGINS_CACHESIM_H
#define S2E_PLUGINS_CACHESIM_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>

#include <string>
#include <map>

namespace s2e {
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

    Cache* getCache(const std::string& name);

public:
    CacheSim(S2E* s2e): Plugin(s2e), m_i1(0), m_d1(0) {}
    ~CacheSim();

    void initialize();
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_CACHESIM_H
