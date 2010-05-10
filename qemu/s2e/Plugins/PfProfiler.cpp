#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/Database.h>
#include <s2e/ConfigFile.h>
#include <s2e/s2e_qemu.h>
#include "PfProfiler.h"
#include "ModuleExecutionDetector.h"
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
          "); create index if not exists pfprofileridx on pfprofiler(moduleid, pc, addr);";
    
    Database *db = s2e()->getDb();
    return db->executeQuery(query);
}

    std::vector<PfProfilerEntry> PfProfilerEntries;

void PfProfiler::flushTable()
{
    s2e()->getDb()->executeQuery("begin transaction;");
    
    foreach2(it, PfProfilerEntries.begin(), PfProfilerEntries.end()) { 
    char buf[512];
    PfProfilerEntry &e = *it;
    snprintf(buf, sizeof(buf), 
        "insert into pfprofiler values("
        "%"PRIu64", '%s', %"PRIu64", %d, %d, %"PRIu64", %"PRIu64");",
          e.ts, e.moduleId.c_str(), e.pid, e.isTlbMiss, e.isWrite,
          e.pc, e.addr);

    Database *db = g_s2e->getDb();
    db->executeQuery(buf);
    }
    s2e()->getDb()->executeQuery("end transaction;");
    PfProfilerEntries.clear();
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
    ModuleExecutionDetector *m_execDetector = 
        (ModuleExecutionDetector*)g_s2e->getPlugin("ModuleExecutionDetector");
    assert(m_execDetector);
    const ModuleDescriptor *desc = m_execDetector->getCurrentDescriptor(state);
   
    uint64_t ts = llvm::sys::TimeValue::now().usec();

    PfProfilerEntry e;
    e.ts = ts;
    e.pc = desc ? desc->ToNativeBase(state->getPc()): state->getPc();
    e.pid = state->getPid();
    e.moduleId = desc ? desc->Name : "";
    e.isTlbMiss = isTlbMiss;
    e.addr = addr;
    e.isWrite = is_write;

    PfProfilerEntries.push_back(e);   
}




}
}