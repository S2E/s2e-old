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

#ifndef S2ETOOLS_CACHEPROFILER_H
#define S2ETOOLS_CACHEPROFILER_H

#include <lib/ExecutionTracer/LogParser.h>
#include <lib/ExecutionTracer/ModuleParser.h>
#include <lib/BinaryReaders/ExecutableFile.h>

#include <lib/BinaryReaders/Library.h>


#include <string>
#include <set>
#include <map>

namespace s2etools
{

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

struct InstructionCacheStatistics
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

class CacheProfilerState;

class CacheProfiler
{
private:
    s2etools::LogEvents *m_Events;
    s2etools::ModuleCache *m_moduleCache;

    sigc::connection m_connection;
    Caches m_caches;
    CacheIdToName m_cacheIds;


    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);

public:

    CacheProfiler(s2etools::ModuleCache *modCache, s2etools::LogEvents *events);
    ~CacheProfiler();

    void printAggregatedStatistics(std::ostream &os) const;
    void printAggregatedStatisticsHtml(std::ostream &os) const;

    LogEvents *getEvents() const {
        return m_Events;
    }

    friend class CacheProfilerState;
};

class CacheProfilerState: public ItemProcessorState
{
private:
    CacheStatisticsMap m_statistics;

    void processCacheItem(CacheProfiler *cp, uint64_t pid, const s2e::plugins::ExecutionTraceCacheSimEntry *e);
public:
    static ItemProcessorState *factory();
    CacheProfilerState();
    virtual ~CacheProfilerState();
    virtual ItemProcessorState *clone() const;

    const CacheStatisticsMap &getStats() const {
        return m_statistics;
    }


    friend class CacheProfiler;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


class TopMissesPerModule
{
public:
    struct SortByTopMissesByModule{
        bool operator () (const InstructionCacheStatistics &s1, const InstructionCacheStatistics &s2) const {
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

    typedef std::set<InstructionCacheStatistics, SortByTopMissesByModule> TopMissesPerModuleSet;

private:
    CacheProfiler *m_Profiler;
    Library *m_library;

    std::string m_filteredProcess;
    std::string m_filteredModule;
    uint64_t m_minCacheMissThreshold;

    //Display debug info for all modules in the trace
    bool m_displayAllModules;

    TopMissesPerModuleSet m_stats;

    uint64_t m_totalMisses;
public:
    TopMissesPerModule(Library *library, CacheProfiler *prof);
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


    void setDisplayAllModules(bool b) {
        m_displayAllModules = true;
    }

    void computeStats(uint32_t pathId);

    //void processCacheItem(const s2e::plugins::ExecutionTraceCacheSimEntry *e);
    void print(std::ostream &os);
    void printAggregatedStatistics(std::ostream &os) const;

    uint64_t getTotalMisses() const {
        return m_totalMisses;
    }
};

}

#endif
