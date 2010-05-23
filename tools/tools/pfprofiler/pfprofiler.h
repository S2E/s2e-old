#ifndef S2ETOOLS_PFPROFILER_H
#define S2ETOOLS_PFPROFILER_H

#include <lib/ExecutionTracer/LogParser.h>

class PfProfiler
{
private:
    std::string m_FileName;
    s2etools::LogParser m_Parser;

    void processCallItem(unsigned traceIndex,
                         const s2e::plugins::ExecutionTraceItemHeader &hdr,
                         const s2e::plugins::ExecutionTraceCall &call);

public:
    PfProfiler(const std::string &file);
    ~PfProfiler();

    void process();
};

#endif
