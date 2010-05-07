extern "C" {
#include <qemu-common.h>
#include <exec-all.h>
}

#include "CacheSim.h"

#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Utils.h>
#include <s2e/Database.h>

#include <llvm/System/TimeValue.h>

#include <iostream>
#include <stdlib.h>
#include <stdio.h>

#define CACHESIM_LOG_SIZE 4096

namespace s2e {
namespace plugins {

using namespace std;
using namespace klee;

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
            bool isWrite, unsigned* misCount, unsigned misCountSize)
    {

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
    flushLogEntries();
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

    //This is optional
    m_execDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");

    if (!m_execDetector) {
        s2e()->getMessagesStream() << "ModuleExecutionDetector not found, will profile the whole system" << std::endl;
    }

    //Option to write only those instructions that cause misses
    m_reportZeroMisses = conf->getBool(getConfigKey() + ".reportZeroMisses");
    m_profileModulesOnly = conf->getBool(getConfigKey() + ".reportZeroMisses");
    bool startOnModuleLoad = conf->getBool(getConfigKey() + ".startOnModuleLoad");

    vector<string> caches = conf->getListKeys(getConfigKey() + ".caches");
    foreach(const string& cacheName, caches) {
        string key = getConfigKey() + ".caches." + cacheName;
        Cache* cache = new Cache(cacheName,
                                 conf->getInt(key + ".size"),
                                 conf->getInt(key + ".associativity"),
                                 conf->getInt(key + ".lineSize"));

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
    for(Cache* c = m_i1; c != NULL; c = c->getUpperCache()) {
        m_i1_length += 1;
        s2e()->getMessagesStream() << " -> " << c->getName();
    }
    s2e()->getMessagesStream() << " -> memory" << std::endl;

    s2e()->getMessagesStream() << "Data cache hierarchy:";
    for(Cache* c = m_d1; c != NULL; c = c->getUpperCache()) {
        m_d1_length += 1;
        s2e()->getMessagesStream() << " -> " << c->getName();
    }
    s2e()->getMessagesStream() << " -> memory" << std::endl;

    if (m_execDetector && startOnModuleLoad){
        m_ModuleConnection = m_execDetector->onModuleTranslateBlockStart.connect(
                sigc::mem_fun(*this, &CacheSim::onModuleTranslateBlockStart));

    }else {
        if(m_d1)
            s2e()->getCorePlugin()->onDataMemoryAccess.connect(
                sigc::mem_fun(*this, &CacheSim::onDataMemoryAccess));

        if(m_i1)
            s2e()->getCorePlugin()->onTranslateBlockStart.connect(
             sigc::mem_fun(*this, &CacheSim::onTranslateBlockStart));
    }

    const char *query = "create table CacheSim("
          "'timestamp' unsigned big int, "
          "'pc' unsigned big int, "
          "'address' unsigned bit int, "
          "'size' unsigned int, "
          "'isWrite' boolean, "
          "'isCode' boolean, "
          "'cacheName' varchar(30), "
          "'missCount' unsigned int"
          "); create index if not exists CacheSimIdx on CacheSim (pc);";

    bool ok = s2e()->getDb()->executeQuery(query);
    assert(ok && "create table failed");

    m_cacheLog.reserve(CACHESIM_LOG_SIZE);

    s2e()->getCorePlugin()->onTimer.connect(
        sigc::mem_fun(*this, &CacheSim::onTimer)
    );
}

//Connect the tracing when the first module is loaded
void CacheSim::onModuleTranslateBlockStart(
    ExecutionSignal* signal, 
    S2EExecutionState *state, 
    const ModuleExecutionDesc*desc,
    TranslationBlock *tb, uint64_t pc)
{
    
    s2e()->getDebugStream() << "Module translation CacheSim " << desc->id << "  " << 
        pc <<std::endl;
    if(m_d1)
        s2e()->getCorePlugin()->onDataMemoryAccess.connect(
            sigc::mem_fun(*this, &CacheSim::onDataMemoryAccess));

    if(m_i1)
        s2e()->getCorePlugin()->onTranslateBlockStart.connect(
            sigc::mem_fun(*this, &CacheSim::onTranslateBlockStart));

    //We connected ourselves, do not need to monitor modules anymore.
    s2e()->getDebugStream()  << "Disconnecting module translation cache sim" << std::endl;
    m_ModuleConnection.disconnect();
}

//Periodically flush the cache
void CacheSim::onTimer()
{
    flushLogEntries();
}

void CacheSim::flushLogEntries()
{
    char query[512];
    bool ok = s2e()->getDb()->executeQuery("begin transaction;");
    assert(ok && "Can not execute database query");
    foreach(const CacheLogEntry& ce, m_cacheLog) {
        snprintf(query, sizeof(query),
             "insert into CacheSim values(%"PRIu64",%"PRIu64",%"PRIu64",%u,%u,%u,'%s',%u);",
             ce.timestamp, ce.pc, ce.address, ce.size,
             ce.isWrite, ce.isCode,
             ce.cacheName, ce.missCount);
        ok = s2e()->getDb()->executeQuery(query);
        assert(ok && "Can not execute database query");
    }
    ok = s2e()->getDb()->executeQuery("end transaction;");
    assert(ok && "Can not execute database query");
    m_cacheLog.resize(0);
}

void CacheSim::onMemoryAccess(S2EExecutionState *state,
                              uint64_t address, unsigned size,
                              bool isWrite, bool isIO, bool isCode)
{
    if(isIO) /* this is only an estimation - should look at registers! */
        return;

    Cache* cache = isCode ? m_i1 : m_d1;
    if(!cache)
        return;

    //Check whether to profile only known modules
    if (m_execDetector && m_profileModulesOnly) {
        const ModuleExecutionDesc *md;
        md = m_execDetector->getCurrentModule(state);
        if (!md) {
            return;
        }
    }
    
    unsigned missCountLength = isCode ? m_i1_length : m_d1_length;
    unsigned missCount[missCountLength];
    memset(missCount, 0, sizeof(missCount));
    cache->access(address, size, isWrite, missCount, missCountLength);

    //Decide whether to log the access in the database
    bool doLog = false;
    if (m_execDetector) {
        const ModuleExecutionDesc *md;
        md = m_execDetector->getCurrentModule(state);
        if (!md) {
            doLog = false;
        }else {
            doLog = true;
        }
    }else {
        doLog = true;
    }

    if (!doLog) {
        return;
    }

    unsigned i = 0;
    for(Cache* c = cache; c != NULL; c = c->getUpperCache(), ++i) {
        if(m_cacheLog.size() == CACHESIM_LOG_SIZE)
            flushLogEntries();

       // std::cout << state->getPc() << " "  << c->getName() << ": " << missCount[i] << std::endl;

        if (m_reportZeroMisses || missCount[i]) {
            m_cacheLog.resize(m_cacheLog.size()+1);
            CacheLogEntry& ce = m_cacheLog.back();
            ce.timestamp = llvm::sys::TimeValue::now().usec();
            ce.pc = state->getPc();
            ce.address = address;
            ce.size = size;
            ce.isWrite = isWrite;
            ce.isCode = false;
            ce.cacheName = c->getName().c_str();
            ce.missCount = missCount[i];
        }

        if(missCount[i] == 0)
            break;
    }
}

void CacheSim::onDataMemoryAccess(S2EExecutionState *state,
                              klee::ref<klee::Expr> address,
                              klee::ref<klee::Expr> value,
                              bool isWrite, bool isIO)
{
    if(!isa<ConstantExpr>(address)) {
        s2e()->getWarningsStream()
                << "Warning: CacheSim do not supports symbolic addresses"
                << std::endl;
        return;
    }

    uint64_t constAddress = cast<ConstantExpr>(address)->getZExtValue(64);
    unsigned size = Expr::getMinBytesForWidth(value->getWidth());

    onMemoryAccess(state, constAddress, size, isWrite, isIO, false);
}

void CacheSim::onExecuteBlockStart(S2EExecutionState *state, uint64_t pc,
                                   TranslationBlock *tb)
{
    onMemoryAccess(state, tb->pc, tb->size, false, false, true);
}

void CacheSim::onTranslateBlockStart(ExecutionSignal *signal,
                                     S2EExecutionState *, TranslationBlock *tb,
                                     uint64_t)
{
    signal->connect(sigc::bind(
            sigc::mem_fun(*this, &CacheSim::onExecuteBlockStart), tb));
}


} // namespace plugins
} // namespace s2e
