#include <iomanip>
#include <iostream>
#include "TestCase.h"

using namespace s2e::plugins;

namespace s2etools {

TestCase::TestCase(LogEvents *events)
{
   m_foundInputs = false;
   m_connection = events->onEachItem.connect(
           sigc::mem_fun(*this, &TestCase::onItem));
}

TestCase::~TestCase()
{
    m_connection.disconnect();
}

void TestCase::onItem(unsigned traceIndex,
        const s2e::plugins::ExecutionTraceItemHeader &hdr,
        void *item)
{
    if (hdr.type != s2e::plugins::TRACE_TESTCASE) {
        return;
    }

    std::cerr << "TestCase stateId=" << hdr.stateId << std::endl;
    if (m_foundInputs) {
        std::cerr << "The execution trace has multiple input sets. Make sure you used the PathBuilder filter."
                <<std::endl;

    }
    ExecutionTraceTestCase::deserialize(item, hdr.size, m_inputs);
    m_foundInputs = true;

}

void TestCase::printInputs(std::ostream &os)
{
    if (!m_foundInputs) {
        os << "No concrete inputs found in the trace. Make sure you used the TestCaseGenerator plugin." <<
                std::endl;
        return;
    }

    ExecutionTraceTestCase::ConcreteInputs::iterator it;
    os << "Concrete inputs:" << std::endl;

    for (it = m_inputs.begin(); it != m_inputs.end(); ++it) {
        const ExecutionTraceTestCase::VarValuePair &vp = *it;
        os << "  " << vp.first << ": ";

        for (unsigned i=0; i<vp.second.size(); ++i) {
            os << std::setw(2) << std::right << std::setfill('0') << (unsigned) vp.second[i] << ' ';
        }

        os << std::setfill(' ')<< std::endl;
    }
}

}
