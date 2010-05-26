#ifndef S2ETOOLS_PFPROFILER_H
#define S2ETOOLS_PFPROFILER_H

#include <lib/ExecutionTracer/LogParser.h>
#include <lib/ExecutionTracer/ModuleParser.h>

#include <ostream>

class CacheParameters
{
private:
    unsigned m_size;
    unsigned m_lineSize;
    unsigned m_associativity;
    std::string m_name;
    CacheParameters *m_upper;

public:
    uint64_t m_TotalMissesOnWrite;
    uint64_t m_TotalMissesOnRead;

public:
    CacheParameters(const std::string &name,
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

    void setUpperCache(CacheParameters *p) {
        m_upper = p;
    }

    void print(std::ostream &os);
};

typedef std::map<uint32_t, CacheParameters *> Caches;
typedef std::map<uint32_t, std::string> CacheIdToName;

class CacheProfiler: public s2etools::LogEvents
{
private:
    Caches m_caches;
    CacheIdToName m_cacheIds;
    s2etools::LogEvents *m_Events;


    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);

    void processCacheItem(const s2e::plugins::ExecutionTraceCacheSimEntry *e);
public:

    CacheProfiler(s2etools::LogEvents *events);
    ~CacheProfiler();

    void printAggregatedStatistics(std::ostream &os) const;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class PfProfiler
{
private:
    std::string m_FileName;
    s2etools::LogParser m_Parser;

    s2etools::ModuleLibrary m_Library;
    s2etools::ModuleCache *m_ModuleCache;


    void processCallItem(unsigned traceIndex,
                         const s2e::plugins::ExecutionTraceItemHeader &hdr,
                         const s2e::plugins::ExecutionTraceCall &call);

    void processModuleLoadItem(unsigned traceIndex,
                         const s2e::plugins::ExecutionTraceItemHeader &hdr,
                         const s2e::plugins::ExecutionTraceModuleLoad &load);


public:
    PfProfiler(const std::string &file);
    ~PfProfiler();

    void process();
};

#endif
