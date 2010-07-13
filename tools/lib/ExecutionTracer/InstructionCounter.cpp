#include <iomanip>
#include <iostream>
#include <cassert>
#include "InstructionCounter.h"

using namespace s2e::plugins;

namespace s2etools {

InstructionCounter::InstructionCounter(LogEvents *events)
{
   m_icount = 0;
   m_connection = events->onEachItem.connect(
           sigc::mem_fun(*this, &InstructionCounter::onItem));
}

InstructionCounter::~InstructionCounter()
{
    m_connection.disconnect();
}

void InstructionCounter::onItem(unsigned traceIndex,
        const s2e::plugins::ExecutionTraceItemHeader &hdr,
        void *item)
{
    if (hdr.type != s2e::plugins::TRACE_ICOUNT) {
        return;
    }

    ExecutionTraceICount *e = static_cast<ExecutionTraceICount*>(item);
    assert(e->count >= m_icount);
    m_icount = e->count;
}

void InstructionCounter::printCounter(std::ostream &os)
{
    os << "Instruction count: " << std::dec << m_icount << std::endl;
}

}
