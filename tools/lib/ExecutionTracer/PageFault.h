#ifndef S2ETOOLS_EXECTRACER_PageFault_H
#define S2ETOOLS_EXECTRACER_PageFault_H

#include <s2e/Plugins/ExecutionTracers/TraceEntries.h>
#include "LogParser.h"
#include "ModuleParser.h"

namespace s2etools {

class PageFault
{
private:
    sigc::connection m_connection;

    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);

    ModuleCache *m_mc;

    uint64_t m_totalTlbMisses;
    uint64_t m_totalPageFaults;

    bool m_trackModule;
    std::string m_module;

public:
    PageFault(LogEvents *events, ModuleCache *mc);
    ~PageFault();

    void setModule(const std::string &s) {
        m_module = s;
        m_trackModule = true;
    }

    uint64_t getPageFaults() const {
        return m_totalPageFaults;
    }

    uint64_t getTlbMisses() const {
        return m_totalTlbMisses;
    }
};

}
#endif

