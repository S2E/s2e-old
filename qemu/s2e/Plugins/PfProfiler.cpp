#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/Database.h>
#include <s2e/ConfigFile.h>
#include <s2e/s2e_qemu.h>
#include "PfProfiler.h"
#include <map>
#include <llvm/System/TimeValue.h>

namespace s2e {
namespace plugins {


S2E_DEFINE_PLUGIN(PfProfiler, "PfProfiler", "",);



void PfProfiler::initialize()
{
    ConfigFile *cfg = s2e()->getConfig();

    m_TrackTlbMisses = cfg->getBool(getConfigKey() + ".trackTlbMisses");
    m_TrackPageFaults = cfg->getBool(getConfigKey() + ".trackPageFaults");
    m_Aggregated = cfg->getBool(getConfigKey() + ".aggregated");
    m_FlushPeriod = cfg->getInt(getConfigKey() + ".flushPeriod");
    
    s2e()->getMessagesStream() << "Tracking TLB misses: " << m_TrackTlbMisses <<
        " Tracking page faults: " << m_TrackPageFaults << std::endl;
    

    bool useBinaryLogFile = cfg->getBool(getConfigKey() + ".useBinaryLogFile");

    if (m_Aggregated && useBinaryLogFile) {
        s2e()->getWarningsStream() << "PfProfiler: cannot use binary log file in aggregated mode." << std::endl;
        exit(-1);
    }

    m_LogFile = NULL;
    if (useBinaryLogFile) {
        s2e()->getMessagesStream() << "PfProfiler: using binary log file" << std::endl;
        m_LogFile = fopen(s2e()->getOutputFilename("PfProfiler.dat").c_str(), "wb");
        assert(m_LogFile);
    }else {
        if (m_Aggregated) {
            if (!createAggregatedTable()) {
                assert(false);
            }
        }else {
            if (!createTable()) {
                assert(false);
            }
        }
    }

    if (!useBinaryLogFile) {
        m_PfProfilerEntries.reserve(100000);

        s2e()->getCorePlugin()->onTimer.connect(
                sigc::mem_fun(*this, m_Aggregated ? (void (PfProfiler::*)()) &PfProfiler::flushAggregatedTable : &PfProfiler::flushTable)
        );
    }

    if (m_TrackPageFaults) {
        s2e()->getCorePlugin()->onPageFault.connect(
            sigc::mem_fun(*this, &PfProfiler::onPageFault)
            );
    }

    if (m_TrackTlbMisses) {
        s2e()->getCorePlugin()->onTlbMiss.connect(
            sigc::mem_fun(*this, &PfProfiler::onTlbMiss)
            );
    }



    m_execDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    assert(m_execDetector);

}

bool PfProfiler::createTable()
{
    const char *query = "create table PfProfiler(" 
          "'timestamp' unsigned big int,"  
          "'moduleId' varchar(30),"
          "'pid' unsigned big int,"
          "'istlbmiss' boolean,"
          "'isWrite' boolean,"
          "'pc' unsigned big int,"
          "'addr' unsigned big int"
          "); create index if not exists pfprofileridx on pfprofiler(pc);";
    
    Database *db = s2e()->getDb();
    return db->executeQuery(query);
}

bool PfProfiler::createAggregatedTable()
{
    const char *query = "create table PfProfileAggrCode("
                        "'moduleId' varchar(30),"
                        "'pid' unsigned big int,"
                        "'pc' unsigned big int,"
                        "'relpc' unsigned big int,"
                        "'natpc' unsigned big int,"
                        "'numAccesses' unsigned int,"
                        "'isPageFault' boolean"
                        "); "; //create index if not exists pfprofileaggrcodeidx on pfprofileaggrcode(pc,pid);";

    Database *db = s2e()->getDb();
    return db->executeQuery(query);
}

void PfProfiler::flushAggregatedTable(const AggregatedMap& aggrMap, bool isPfMap)
{
    Database *db = s2e()->getDb();

    foreach2(it, aggrMap.begin(), aggrMap.end()) {
        const PfProfileAggrEntry &e = (*it).second;
        char query[256];
#if 0
        snprintf(query, sizeof(query), "update PfProfileAggrCode set numAccesses=%"PRIu64" "
                 "where pc=%"PRIu64" and pid=%"PRIu64";", e.count, e.loadpc, e.pid);
        bool res = db->executeQuery(query);
        assert(res);
        int updatedCount = db->getCountOfChanges();
        assert (updatedCount == 1 || updatedCount == 0);

        if (updatedCount == 0)
#endif
        {
            //Row with specified id does not exist, need to create a new one.
            snprintf(query, sizeof(query), "insert into PfProfileAggrCode values("
                     "'%s', %"PRIu64", %"PRIu64", %"PRIu64", %"PRIu64", %"PRIu64", %d"
                     ");", e.moduleId, e.pid, e.loadpc, e.relativePc, e.nativePc, e.count, isPfMap);
            bool res = db->executeQuery(query);
            assert(res);
        }
    }

}

void PfProfiler::flushAggregatedTable()
{

    s2e()->getDb()->executeQuery("begin transaction;");
    s2e()->getDb()->executeQuery("delete from PfProfileAggrCode;");

    if (m_TrackPageFaults) {
        flushAggregatedTable(m_AggrPageFaults, 1);
    }

    if (m_TrackTlbMisses) {
        flushAggregatedTable(m_AggrMisses, 0);
    }

    s2e()->getDb()->executeQuery("end transaction;");
}

void PfProfiler::flushTable()
{
    static int counter = 0;

    counter++;
    if ((counter % m_FlushPeriod) != 0) {
        return;
    }

    unsigned entriesCount = m_PfProfilerEntries.size();
    unsigned current = 0;

    s2e()->getDb()->executeQuery("begin transaction;");
    
    Database *db = s2e()->getDb();

    foreach2(it, m_PfProfilerEntries.begin(), m_PfProfilerEntries.end()) {
        char buf[512];
        PfProfilerEntry &e = *it;

        snprintf(buf, sizeof(buf),
            "insert into pfprofiler values("
            "%"PRIu64", '%s', %"PRIu64", %d, %d, %"PRIu64", %"PRIu64");",
            e.ts, e.moduleId, e.pid, e.isTlbMiss, e.isWrite,
              e.pc, e.addr);

        db->executeQuery(buf);
        ++current;

        if ((current & 0xFFFF) == 0) {
            s2e()->getDebugStream() << "PfProfiler written " << current << "/"
                    << entriesCount << " entries" << std::endl;
        }
    }
    s2e()->getDb()->executeQuery("end transaction;");

    m_PfProfilerEntries.clear();
}

void PfProfiler::onTlbMiss(S2EExecutionState *state, uint64_t addr, bool is_write)
{
    if (m_Aggregated) {
        missFaultHandlerAggregated(state, true, addr, is_write);
    }else {
        missFaultHandler(state, true, addr, is_write);
    }
}

void PfProfiler::onPageFault(S2EExecutionState *state, uint64_t addr, bool is_write)
{
    if (m_Aggregated) {
        missFaultHandlerAggregated(state, false, addr, is_write);
    }else {
        missFaultHandler(state, false, addr, is_write);
    }
}


void PfProfiler::missFaultHandlerAggregated(S2EExecutionState *state, bool isTlbMiss, uint64_t addr, bool is_write)
{
    AggregatedMap &aggrMap = isTlbMiss ? m_AggrMisses : m_AggrPageFaults;

    const ModuleDescriptor *desc = m_execDetector->getCurrentDescriptor(state);

    AggregatedMap::iterator it = aggrMap.find(PidPcPair(state->getPc(), state->getPid()));
    if (it == aggrMap.end()) {
        PfProfileAggrEntry e;
        strncpy(e.moduleId,  desc ? desc->Name.c_str() : "", sizeof(e.moduleId)-1);
        e.count = 1;
        e.loadpc = state->getPc();
        e.nativePc = desc ? desc->ToNativeBase(state->getPc()): 0;
        e.relativePc = desc ? desc->ToRelative(state->getPc()): 0;
        e.pid = state->getPid();
        aggrMap[PidPcPair(state->getPc(), state->getPid())] = e;
    }else {
        PfProfileAggrEntry &e = (*it).second;
        ++e.count;
        assert(e.pid == state->getPid());
        assert(e.loadpc == state->getPc());
    }
}

void PfProfiler::missFaultHandler(S2EExecutionState *state, bool isTlbMiss, uint64_t addr, bool is_write)
{
    const ModuleDescriptor *desc = m_execDetector->getCurrentDescriptor(state);
   
    uint64_t ts = llvm::sys::TimeValue::now().usec();

    PfProfilerEntry e;
    e.ts = ts;
    e.pc = state->getPc();
    assert(e.pc < 0x100000000);
    e.relPc = desc ? desc->ToRelative(state->getPc()): 0;
    e.pid = state->getPid();
    strncpy(e.moduleId,  desc ? desc->Name.c_str() : "", sizeof(e.moduleId)-1);
    e.isTlbMiss = isTlbMiss;
    e.addr = addr;
    e.isWrite = is_write;

    if (m_LogFile) {
        fwrite(&e, sizeof(e), 1, m_LogFile);
    }else {
        m_PfProfilerEntries.push_back(e);
    }
}




}
}
