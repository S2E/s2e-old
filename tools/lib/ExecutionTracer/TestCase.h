#ifndef S2ETOOLS_EXECTRACER_TESTCASE_H
#define S2ETOOLS_EXECTRACER_TESTCASE_H

#include <s2e/plugins/ExecutionTracers/TraceEntries.h>
#include "LogParser.h"

namespace s2etools {

class TestCase:public LogEvents
{
private:
    sigc::connection m_connection;
    s2e::plugins::ExecutionTraceTestCase::ConcreteInputs m_inputs;
    bool m_foundInputs;

    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);
public:
    TestCase(LogEvents *events);

    ~TestCase();

    bool getInputs(const s2e::plugins::ExecutionTraceTestCase::ConcreteInputs &out) const;
    void printInputs(std::ostream &os);
};

}
#endif

