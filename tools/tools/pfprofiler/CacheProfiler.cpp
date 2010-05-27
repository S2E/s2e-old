#include <iomanip>
#include <iostream>
#include "CacheProfiler.h"
#include <lib/BinaryReaders/BFDInterface.h>


using namespace s2e::plugins;
using namespace s2etools;

void Cache::print(std::ostream &os)
{
    os << std::dec;
    os << "Cache " << m_name << " - Statistics" << std::endl;
    os << "Total Read  Misses: " << m_TotalMissesOnRead << std::endl;
    os << "Total Write Misses: " << m_TotalMissesOnWrite << std::endl;
    os << "Total       Misses: " << m_TotalMissesOnRead + m_TotalMissesOnWrite << std::endl;
}

CacheProfiler::CacheProfiler(ModuleCache *modCache, LogEvents *events)
{
    m_moduleCache = modCache;
    m_Events = events;
    m_connection = events->onEachItem.connect(
            sigc::mem_fun(*this, &CacheProfiler::onItem)
            );
}

CacheProfiler::~CacheProfiler()
{
    Caches::iterator it;
    m_connection.disconnect();
    for (it = m_caches.begin(); it != m_caches.end(); ++it) {
        delete (*it).second;
    }
}

void CacheProfiler::processCacheItem(uint64_t pid, const ExecutionTraceCacheSimEntry *e)
{
    Caches::iterator it = m_caches.find(e->cacheId);
    assert(it != m_caches.end());

    Cache *c = (*it).second;

    if (e->missCount > 0) {
        if (e->isWrite) {
            c->m_TotalMissesOnWrite += e->missCount;
        }else {
            c->m_TotalMissesOnRead += e->missCount;
        }
    }

    CacheStatisticsEx s;
    const ModuleInstance *modInst = m_moduleCache->getInstance(pid, e->pc);
    s.instr.m = modInst->Mod;
    s.instr.loadBase = modInst->LoadBase;
    s.instr.pid = pid;
    s.instr.pc = e->pc;
    s.stats.c = c;

    if (e->isWrite) {
        s.stats.writeMissCount = e->missCount;
    }else {
        s.stats.readMissCount = e->missCount;
    }

    //Update the per-instruction statistics
    CacheStatisticsMap::iterator cssit = m_statistics.find(s.instr);
    if (cssit == m_statistics.end()) {
        m_statistics[s.instr] = s.stats;
    }else {
        if (e->isWrite) {
            (*cssit).second.writeMissCount += e->missCount;
        }else {
            (*cssit).second.readMissCount += e->missCount;
        }
    }
}

void CacheProfiler::onItem(unsigned traceIndex,
            const s2e::plugins::ExecutionTraceItemHeader &hdr,
            void *item)
{
    //std::cout << "Processing entry " << std::dec << traceIndex << " - " << (int)hdr.type << std::endl;

    if (hdr.type != s2e::plugins::TRACE_CACHESIM) {
        return;
    }



    ExecutionTraceCache *e = (ExecutionTraceCache*)item;


    if (e->type == s2e::plugins::CACHE_NAME) {
        std::string s((const char*)e->name.name, e->name.length);
        m_cacheIds[e->name.id] = s;
    }else if (e->type == s2e::plugins::CACHE_PARAMS) {
        CacheIdToName::iterator it = m_cacheIds.find(e->params.cacheId);
        assert(it != m_cacheIds.end());

        Cache *params = new Cache((*it).second, e->params.lineSize, e->params.size,
                                                      e->params.associativity);

        m_caches[e->params.cacheId] = params;
        //XXX: fix that when needed
        //params->setUpperCache(NULL);
    }else if (e->type == s2e::plugins::CACHE_ENTRY) {
        const ExecutionTraceCacheSimEntry *se = &e->entry;
        if (se->pc == 0x401215) {
            std::cout << "Processing entry " << std::dec << traceIndex << " - " << (int)hdr.type << std::endl;
        }

        processCacheItem(hdr.pid, se);
    }else {
        assert(false && "Unknown cache trace entry");
    }
}

void CacheProfiler::printAggregatedStatistics(std::ostream &os) const
{
    Caches::const_iterator it;

    for(it = m_caches.begin(); it != m_caches.end(); ++it) {
        (*it).second->print(os);
        os << "-------------------------------------" << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TopMissesPerModule::TopMissesPerModule(CacheProfiler *prof)
{
    m_Profiler = prof;
}

/*void TopMissesPerModule::processCacheItem(const s2e::plugins::ExecutionTraceCacheSimEntry *e)
{

}*/

void TopMissesPerModule::print(std::ostream &os, const std::string libpath)
{
    std::string source, func;
    uint64_t line;

    std::string ProgFile = libpath + "/matrix.exe";
    BFDInterface iface(ProgFile);
    //iface.getInfo(0x004013D6, source, line, func);



    const CacheStatisticsMap &stats = m_Profiler->getStats();
    CacheStatisticsMap::const_iterator it;
    TopMissesPerModuleSet sorted;

    //Sort all the elements by total misses
    for(it = stats.begin(); it != stats.end(); ++it) {
        CacheStatisticsEx ex;
        ex.instr = (*it).first;
        ex.stats = (*it).second;
        sorted.insert(ex);
    }


    TopMissesPerModuleSet::const_reverse_iterator sit;
    os << std::setw(10) << std::left << "Module" << std::setw(10) << " PC" <<
            std::setw(6) << "    ReadMissCount" <<
            std::setw(6) << " WriteMissCount" <<
            std::endl;

    for (sit = sorted.rbegin(); sit != sorted.rend(); ++sit) {
        const CacheStatisticsEx &s = (*sit);
        if (s.stats.readMissCount + s.stats.writeMissCount < 10) {
            //continue;
        }

        os << std::setw(10) << s.instr.m->getModuleName() << std::hex
                << " 0x" << std::setw(8) << s.instr.pc << " - ";
        //os << std::hex << std::right << std::setfill('0') << "0x" << std::setw(8) << s.instr.pid
        //                                    << " 0x" << std::setw(8) << s.instr.pc << " - ";
        os << std::setfill(' ');
        os << std::dec  << std::setw(13)<< s.stats.readMissCount
                 << " " << std::setw(14)<< s.stats.writeMissCount;

        uint64_t reladdr = s.instr.pc - s.instr.loadBase + s.instr.m->getImageBase();
        if (iface.getInfo(reladdr, source, line, func)) {
            os << " - " << source << ":" << line << " - " << func;
        }
        os << std::endl;
    }
}
