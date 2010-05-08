#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/Database.h>
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
    if (!createTable()) {
        assert(false);
    }

    s2e()->getCorePlugin()->onTimer.connect(
        sigc::mem_fun(*this, &PfProfiler::flushTable)
    );
}

bool PfProfiler::createTable()
{
    const char *query = "create table PfProfiler(" 
          "'timestamp' unsigned big int,"  
          "'moduleId' varchar(30),"
          "'istlbmiss' unsigned big int,"
          "'pc' unsigned big int,"
          "'pid' unsigned big int"
          "); create index if not exists pfprofileridx on pfprofiler(moduleid, pc);";
    
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
    snprintf(buf, sizeof(buf), "insert into pfprofiler values(%"PRIu64", '%s', %d, "
        "%"PRIu64", %"PRIu64");", e.ts, e.moduleId.c_str(), e.isTlbMiss,
          e.pc, e.pid);

    Database *db = g_s2e->getDb();
    db->executeQuery(buf);
    }
    s2e()->getDb()->executeQuery("end transaction;");
    PfProfilerEntries.clear();
}


typedef std::pair<uint64_t, uint64_t> PidPcPair;
    static std::map<PidPcPair , uint64_t> tlbMisses;
    static std::map<PidPcPair , uint64_t> pageFaults;
    


extern "C" {



void tlb_fault_miss_increment(int isTlbMiss)
{
    ModuleExecutionDetector *m_execDetector = 
        (ModuleExecutionDetector*)g_s2e->getPlugin("ModuleExecutionDetector");
    assert(m_execDetector);
    const ModuleExecutionDesc *desc = m_execDetector->getCurrentModule(g_s2e_state);
   
    uint64_t ts = llvm::sys::TimeValue::now().usec();

    PfProfilerEntry e;
    e.ts = ts;
    e.pc = g_s2e_state->getPc();
    e.pid = g_s2e_state->getPid();
    e.moduleId = desc ? desc->id : "";
    e.isTlbMiss = isTlbMiss;

    PfProfilerEntries.push_back(e);

   
}

void page_fault_increment()
{
    tlb_fault_miss_increment(0);
}

void tlb_miss_increment()
{
    //tlb_fault_miss_increment(1);
}

}

}
}