#include <iomanip>
#include <iostream>
#include <cassert>
#include "PageFault.h"

using namespace s2e::plugins;

namespace s2etools {

PageFault::PageFault(LogEvents *events, ModuleCache *mc)
{
   m_trackModule = false;
   m_totalTlbMisses = 0;
   m_totalPageFaults = 0;
   m_connection = events->onEachItem.connect(
           sigc::mem_fun(*this, &PageFault::onItem));
   m_mc = mc;
}

PageFault::~PageFault()
{
    m_connection.disconnect();
}

void PageFault::onItem(unsigned traceIndex,
        const s2e::plugins::ExecutionTraceItemHeader &hdr,
        void *item)
{
    if (hdr.type == s2e::plugins::TRACE_PAGEFAULT) {
        ExecutionTracePageFault *pageFault = (ExecutionTracePageFault*)item;
        if (m_trackModule) {
            const ModuleInstance *mi = m_mc->getInstance(hdr.pid, pageFault->pc);
            if (!mi || mi->Mod->getModuleName() != m_module) {
                return;
            }
            m_totalPageFaults++;
        }
    }

    if (hdr.type == s2e::plugins::TRACE_TLBMISS) {
        ExecutionTracePageFault *tlbMiss = (ExecutionTracePageFault*)item;
        if (m_trackModule) {
            const ModuleInstance *mi = m_mc->getInstance(hdr.pid, tlbMiss->pc);
            if (!mi || mi->Mod->getModuleName() != m_module) {
                return;
            }
            m_totalTlbMisses++;
        }
    }


}



}
