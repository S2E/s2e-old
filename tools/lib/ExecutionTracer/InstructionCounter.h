#ifndef S2ETOOLS_EXECTRACER_ICOUNT_H
#define S2ETOOLS_EXECTRACER_ICOUNT_H

#include <s2e/Plugins/ExecutionTracers/TraceEntries.h>
#include "LogParser.h"

namespace s2etools {

class InstructionCounter:public LogEvents
{
private:
    sigc::connection m_connection;
    uint64_t m_icount;

    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);
public:
    InstructionCounter(LogEvents *events);

    ~InstructionCounter();

    void printCounter(std::ostream &os);
    uint64_t getCount() const {
        return m_icount;
    }
};

}
#endif

