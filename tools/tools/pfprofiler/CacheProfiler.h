#ifndef S2ETOOLS_CACHEPROFILER_H
#define S2ETOOLS_CACHEPROFILER_H

#include <lib/ExecutionTracer/LogParser.h>
#include <lib/ExecutionTracer/ModuleParser.h>
#include <lib/BinaryReaders/BFDInterface.h>


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

    std::string getName() const {
        return m_name;
    }

    uint64_t getTotalWriteMisses()const {
        return m_TotalMissesOnWrite;
    }

    uint64_t getTotalReadMisses()const {
        return m_TotalMissesOnRead;
    }
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


struct CacheStatistics
{
    Cache *c;
    uint64_t readMissCount;
    uint64_t writeMissCount;

    CacheStatistics() {
        c = NULL;
        readMissCount = writeMissCount = 0;
    }

    CacheStatistics operator +(const CacheStatistics&r) {
        CacheStatistics ret;
        ret.readMissCount = readMissCount + r.readMissCount;
        ret.writeMissCount = writeMissCount + r.writeMissCount;
        return ret;
    }

    CacheStatistics& operator +=(const CacheStatistics&r) {
        readMissCount += r.readMissCount;
        writeMissCount += r.writeMissCount;
        return *this;
    }

    void printHtml(std::ostream &os) const;
};

struct CacheStatisticsEx
{
    s2etools::InstructionDescriptor instr;
    CacheStatistics stats;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


typedef std::map<uint32_t, Cache *> Caches;
typedef std::map<uint32_t, std::string> CacheIdToName;
typedef std::pair<s2etools::InstructionDescriptor, Cache*> InstrCachePair;
typedef std::map<InstrCachePair, CacheStatistics> CacheStatisticsMap;

class CacheProfiler: public s2etools::LogEvents
{
private:
    s2etools::LogEvents *m_Events;
    s2etools::ModuleCache *m_moduleCache;

    sigc::connection m_connection;
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
    void printAggregatedStatisticsHtml(std::ostream &os) const;

    const CacheStatisticsMap &getStats() const {
        return m_statistics;
    }
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


class TopMissesPerModule
{
public:
    struct SortByTopMissesByModule{
        bool operator () (const CacheStatisticsEx &s1, const CacheStatisticsEx &s2) const {
            if ((s1.stats.writeMissCount+s1.stats.readMissCount) !=
                (s2.stats.writeMissCount+s2.stats.readMissCount)) {
                return (s1.stats.writeMissCount+s1.stats.readMissCount) <
                        (s2.stats.writeMissCount+s2.stats.readMissCount);
            }

            if (s1.stats.c != s2.stats.c) {
                return s1.stats.c < s2.stats.c;
            }


            return s1.instr < s2.instr;
        }
    };

    typedef std::set<CacheStatisticsEx, SortByTopMissesByModule> TopMissesPerModuleSet;
    typedef std::map<std::string, s2etools::BFDInterface*> ModuleNameToBfd;

private:
    CacheProfiler *m_Profiler;
    std::string m_filteredProcess;
    std::string m_filteredModule;
    uint64_t m_minCacheMissThreshold;
    bool m_html;

    //Where to look for the modules to display
    std::string m_libpath;

    //Will display the debug info for all the specified modules
    ModuleNameToBfd m_displayModules;

    //Display debug info for all modules in the trace
    bool m_displayAllModules;

    TopMissesPerModuleSet m_stats;
public:
    TopMissesPerModule(CacheProfiler *prof);
    ~TopMissesPerModule();

    void setFilteredProcess(const std::string &proc) {
        m_filteredProcess = proc;
    }


    void setFilteredModule(const std::string &proc) {
        m_filteredModule = proc;
    }

    void setMinMissThreshold(uint64_t v) {
        m_minCacheMissThreshold = v;
    }

    void setHtml(bool b) {
        m_html = b;
    }

    void setLibraryPath(const std::string &path) {
        m_libpath = path;
    }

    bool addModuleToDisplay(const std::string &s);
    void setDisplayAllModules(bool b) {
        m_displayAllModules = true;
    }

    void computeStats();

    //void processCacheItem(const s2e::plugins::ExecutionTraceCacheSimEntry *e);
    void print(std::ostream &os);
    void printAggregatedStatistics(std::ostream &os) const;

};

#endif
