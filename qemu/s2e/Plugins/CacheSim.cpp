#include "CacheSim.h"

#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>
#include <stdlib.h>

namespace s2e {
namespace plugins {

using namespace std;

/** Returns the floor form of binary logarithm for a 32 bit integer.
    (unsigned) -1 is returned if n is 0. */
unsigned floorLog2(unsigned n) {
    int pos = 0;
    if (n >= 1<<16) { n >>= 16; pos += 16; }
    if (n >= 1<< 8) { n >>=  8; pos +=  8; }
    if (n >= 1<< 4) { n >>=  4; pos +=  4; }
    if (n >= 1<< 2) { n >>=  2; pos +=  2; }
    if (n >= 1<< 1) {           pos +=  1; }
    return ((n == 0) ? ((unsigned)-1) : pos);
}

/* Model of n-way accosiative write-through LRU cache */
class Cache {
protected:
    uint64_t m_size;
    unsigned m_associativity;
    unsigned m_lineSize;

    unsigned m_indexShift; // log2(m_lineSize)
    unsigned m_indexMask;  // 1 - setsCount

    unsigned m_tagShift;   // m_indexShift + log2(setsCount)

    std::vector<uint64_t> m_lines;

    std::string m_name;
    Cache* m_upperCache;

public:
    Cache(const std::string& name,
          unsigned size, unsigned associativity,
          unsigned lineSize, unsigned cost = 1, Cache* upperCache = NULL)
        : m_size(size), m_associativity(associativity), m_lineSize(lineSize),
          m_name(name), m_upperCache(upperCache)
    {
        assert(size && associativity && lineSize);

        assert(unsigned(1<<floorLog2(associativity)) == associativity);
        assert(unsigned(1<<floorLog2(lineSize)) == lineSize);

        unsigned setsCount = (size / lineSize) / associativity;
        assert(setsCount && unsigned(1 << floorLog2(setsCount)) == setsCount);

        m_indexShift = floorLog2(m_lineSize);
        m_indexMask = setsCount-1;

        m_tagShift = floorLog2(setsCount) + m_indexShift;

        m_lines.resize(setsCount * associativity, (uint64_t) -1);
    }

    const std::string getName() const { return m_name; }

    Cache* getUpperCache() { return m_upperCache; }
    void setUpperCache(Cache* cache) { m_upperCache = cache; }

    /** Models a cache access. A misCount is an array for miss counts (will be
        passed to the upper caches), misCountSize is its size. Array
        must be zero-initialized. */
    void access(uint64_t address, unsigned size,
            bool isWrite, unsigned* misCount, unsigned misCountSize) {

        uint64_t s1 = address >> m_indexShift;
        uint64_t s2 = (address+size-1) >> m_indexShift;

        if(s1 != s2) {
            /* Cache access spawns multiple lines */
            unsigned size1 = m_lineSize - (address & (m_lineSize - 1));
            access(address, size1, isWrite, misCount, misCountSize);
            access((address & ~(m_lineSize-1)) + m_lineSize, size-size1,
                                   isWrite, misCount, misCountSize);
            return;
        }

        unsigned set = s1 & m_indexMask;
        unsigned l = set * m_associativity;
        uint64_t tag = address >> m_tagShift;

        for(unsigned i = 0; i < m_associativity; ++i) {
            if(m_lines[l + i] == tag) {
                /* Cache hit. Move line to MRU. */
                for(unsigned j = i; j > 0; --j)
                    m_lines[l + j] = m_lines[l + j - 1];
                m_lines[l] = tag;
                return;
            }
        }

        /* Cache miss. Install new tag as MRU */
        misCount[0] += 1;
        for(unsigned j = m_associativity-1; j > 0; --j)
            m_lines[l + j] = m_lines[l + j - 1];
        m_lines[l] = tag;

        if(m_upperCache) {
            assert(misCountSize > 1);
            m_upperCache->access(address, size, isWrite,
                                 misCount+1, misCountSize-1);
        }
    }

};

S2E_DEFINE_PLUGIN(CacheSim, "Cache simulator", "",);

CacheSim::~CacheSim()
{
    foreach(const CachesMap::value_type& ci, m_caches)
        delete ci.second;
}

inline Cache* CacheSim::getCache(const string& name)
{
    CachesMap::iterator it = m_caches.find(name);
    if(it == m_caches.end()) {
        cerr << "ERROR: cache " << name << " undefined" << endl;
        exit(1);
    }
    return it->second;
}

void CacheSim::initialize()
{
    ConfigFile* conf = s2e()->getConfig();

    vector<string> caches = conf->getListKeys(getConfigKey() + ".caches");
    foreach(const string& cacheName, caches) {
        string key = getConfigKey() + ".caches." + cacheName;
        Cache* cache = new Cache(cacheName,
                                 conf->getInt(key + ".size"),
                                 conf->getInt(key + ".lineSize"),
                                 conf->getInt(key + ".associativity"));
        m_caches.insert(make_pair(cacheName, cache));
    }

    foreach(const CachesMap::value_type& ci, m_caches) {
        string key = getConfigKey() + ".caches." + ci.first + ".upper";
        if(conf->hasKey(key))
            ci.second->setUpperCache(getCache(conf->getString(key)));
    }

    if(conf->hasKey(getConfigKey() + ".i1"))
        m_i1 = getCache(conf->getString(getConfigKey() + ".i1"));

    if(conf->hasKey(getConfigKey() + ".d1"))
        m_d1 = getCache(conf->getString(getConfigKey() + ".d1"));

    s2e()->getMessagesStream() << "Instruction cache hierarchy:";
    for(Cache* c = m_i1; c != NULL; c = c->getUpperCache())
        s2e()->getMessagesStream() << " -> " << c->getName();
    s2e()->getMessagesStream() << " -> memory" << std::endl;

    s2e()->getMessagesStream() << "Data cache hierarchy:";
    for(Cache* c = m_d1; c != NULL; c = c->getUpperCache())
        s2e()->getMessagesStream() << " -> " << c->getName();
    s2e()->getMessagesStream() << " -> memory" << std::endl;
}

} // namespace plugins
} // namespace s2e
