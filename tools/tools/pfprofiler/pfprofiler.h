#ifndef S2ETOOLS_PFPROFILER_H
#define S2ETOOLS_PFPROFILER_H

#include <lib/ExecutionTracer/LogParser.h>
#include <lib/ExecutionTracer/ModuleParser.h>

class PfProfiler
{
private:
    std::string m_FileName;
    s2etools::LogParser m_Parser;

    s2etools::ModuleLibrary m_Library;
    s2etools::ModuleCache *m_ModuleCache;


    void processCallItem(unsigned traceIndex,
                         const s2e::plugins::ExecutionTraceItemHeader &hdr,
                         const s2e::plugins::ExecutionTraceCall &call);

    void processModuleLoadItem(unsigned traceIndex,
                         const s2e::plugins::ExecutionTraceItemHeader &hdr,
                         const s2e::plugins::ExecutionTraceModuleLoad &load);


public:
    PfProfiler(const std::string &file);
    ~PfProfiler();

    void process();
};

#endif
