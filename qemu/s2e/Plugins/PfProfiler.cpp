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
    
    s2e()->getMessagesStream() << "Tracking TLB misses: " << m_TrackTlbMisses <<
        " Tracking page faults: " << m_TrackPageFaults << std::endl;
    
    if (!createTable()) {
        assert(false);
    }

    s2e()->getCorePlugin()->onTimer.connect(
        sigc::mem_fun(*this, &PfProfiler::flushTable)
    );

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

    m_PfProfilerEntries.reserve(100000);

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

   

void PfProfiler::flushTable()
{
    static int counter = 0;

    counter++;
    if (counter % 180 != 0) {
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
    missFaultHandler(state, true, addr, is_write);
}

void PfProfiler::onPageFault(S2EExecutionState *state, uint64_t addr, bool is_write)
{
    missFaultHandler(state, false, addr, is_write);
}


void PfProfiler::missFaultHandler(S2EExecutionState *state, bool isTlbMiss, uint64_t addr, bool is_write)
{
    const ModuleDescriptor *desc = m_execDetector->getCurrentDescriptor(state);
   
    uint64_t ts = llvm::sys::TimeValue::now().usec();

    PfProfilerEntry e;
    e.ts = ts;
    assert(e.pc < 0x100000000);
    e.pc = desc ? desc->ToNativeBase(state->getPc()): state->getPc();
    assert(e.pc < 0x100000000);
    e.pid = state->getPid();
    strncpy(e.moduleId,  desc ? desc->Name.c_str() : "", sizeof(e.moduleId));
    e.isTlbMiss = isTlbMiss;
    e.addr = addr;
    e.isWrite = is_write;

    m_PfProfilerEntries.push_back(e);
}




}
}
