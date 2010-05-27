#ifndef S2ETOOLS_CACHEPROFILER_H
#define S2ETOOLS_CACHEPROFILER_H

#include <lib/ExecutionTracer/LogParser.h>
#include <lib/ExecutionTracer/ModuleParser.h>

#include <string>
#include <set>
#include <map>

class Cache
{
private:
    unsigned m_size;
    unsigned m_lineSize;
    unsigned m_associativity;
    std::string m_name;
    Cache *m_upper;

public:
    uint64_t m_TotalMissesOnWrite;
    uint64_t m_TotalMissesOnRead;

public:
    Cache(const std::string &name,
                    unsigned lineSize, unsigned size, unsigned assoc)
    {
        m_size = size;
        m_lineSize = lineSize;
        m_associativity = assoc;
        m_name = name;
        m_upper = NULL;

        m_TotalMissesOnRead = 0;
        m_TotalMissesOnWrite = 0;
    }

    void setUpperCache(Cache *p) {
        m_upper = p;
    }

    void print(std::ostream &os);
};




struct CacheStatistics
{
    Cache *c;
    uint64_t readMissCount;
    uint64_t writeMissCount;

    CacheStatistics() {
        c = NULL;
        readMissCount = writeMissCount = 0;
    }
};

struct CacheStatisticsEx
{
    s2etools::InstructionDescriptor instr;
    CacheStatistics stats;
};

typedef std::map<uint32_t, Cache *> Caches;
typedef std::map<uint32_t, std::string> CacheIdToName;
typedef std::map<s2etools::InstructionDescriptor, CacheStatistics> CacheStatisticsMap;

class CacheProfiler: public s2etools::LogEvents
{
private:
    s2etools::LogEvents *m_Events;
    s2etools::ModuleCache *m_moduleCache;

    Caches m_caches;
    CacheIdToName m_cacheIds;
    CacheStatisticsMap m_statistics;

    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);

    void processCacheItem(uint64_t pid, const s2e::plugins::ExecutionTraceCacheSimEntry *e);
public:

    CacheProfiler(s2etools::ModuleCache *modCache, s2etools::LogEvents *events);
    ~CacheProfiler();

    void printAggregatedStatistics(std::ostream &os) const;

    const CacheStatisticsMap &getStats() const {
        return m_statistics;
    }
};


//Should eventually extend the event counter interface
class CacheMissCounter
{
    std::set<CacheStatistics> m_perModuleStats;
    std::map<Cache *, uint64_t> m_readMissCount;
};

class Aggregator
{
public:
    virtual void processCacheItem(const s2e::plugins::ExecutionTraceCacheSimEntry *e) = 0;
    virtual void print(std::ostream &os) = 0;
};


class TopMissesPerModule
{
public:
    struct SortByTopMissesByModule{
        bool operator () (const CacheStatisticsEx &s1, const CacheStatisticsEx &s2) const {
            if (s1.stats.writeMissCount+s1.stats.readMissCount !=
                s2.stats.writeMissCount+s2.stats.readMissCount) {
                return s1.stats.writeMissCount+s1.stats.readMissCount <
                        s2.stats.writeMissCount+s2.stats.readMissCount;
            }

            return s1.instr < s2.instr;
        }
    };

    typedef std::set<CacheStatisticsEx, SortByTopMissesByModule> TopMissesPerModuleSet;

private:
    CacheProfiler *m_Profiler;

public:
    TopMissesPerModule(CacheProfiler *prof);

    //void processCacheItem(const s2e::plugins::ExecutionTraceCacheSimEntry *e);
    void print(std::ostream &os, const std::string libpath);

};

#endif
